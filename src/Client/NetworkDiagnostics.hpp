#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <numeric>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Protocol/SnapshotPayload.hpp"

namespace rtype::client {
/**
 * @addtogroup api_client
 * @{
 */

/**
 * @brief Measures what the player feels of the network, over the last seconds.
 *
 * The counters of the network layer (round trip time, loss, bandwidth) say how
 * the link behaves. This class measures what comes out of it for the game:
 * how often the world is updated, how late the server confirms an input, how
 * far the predicted ship is from the server, how long a lost creation takes to
 * be repaired. It is the data of the figures of the network benchmark, shown
 * live by NetworkHudSystem.
 *
 * Every time is in seconds, taken from the same clock.
 */
class NetworkDiagnostics {
public:
  /** @brief Length of the window of the statistics, in seconds. */
  static constexpr double WINDOW_SECONDS = 10.0;

  /** @brief Summary of the samples of a window. */
  struct Summary {
    /** @brief Most recent sample. */
    double last = 0.0;
    /** @brief Median. */
    double p50 = 0.0;
    /** @brief 95th percentile. */
    double p95 = 0.0;
    /** @brief 99th percentile. */
    double p99 = 0.0;
    /** @brief Largest sample. */
    double max = 0.0;
    /** @brief Number of samples in the window. */
    std::size_t count = 0;
  };

  /** @brief Everything measured, ready to display. */
  struct Report {
    /** @brief True once a world update was received. */
    bool hasSnapshots = false;
    /** @brief World updates received per second, over the last two seconds. */
    double snapshotsPerSecond = 0.0;
    /** @brief Wait between two world updates, in milliseconds (normally 50). */
    Summary updateWaitMs;
    /** @brief Share of the normal world update rate received, in percent. */
    double updatesReceivedPercent = 100.0;
    /** @brief Average number of entities in a snapshot message. */
    double entitiesPerSnapshot = 0.0;
    /** @brief Time between an input and the snapshot that accounts for it. */
    Summary confirmedInputMs;
    /** @brief Distance between the predicted ship and the server, in pixels. */
    Summary predictionGapPx;
    /** @brief Time to repair an entity listed before its creation. */
    Summary creationRepairMs;
    /** @brief Entities listed in a snapshot whose creation did not arrive. */
    std::size_t pendingCreations = 0;
  };

  /** @brief Forgets everything, at the start of a game. */
  void reset() {
    _frames.clear();
    _inputs.clear();
    _updateWait.clear();
    _confirmed.clear();
    _gap.clear();
    _repair.clear();
    _entities.clear();
    _unknown.clear();
    _hasFrame = false;
  }

  /**
   * @brief Records that the client declared its actions for an input tick.
   * @param tick Input tick of the frame.
   * @param now Current time.
   */
  void onInput(std::uint32_t tick, double now) {
    if (!_inputs.empty() && _inputs.back().first == tick)
      return;
    _inputs.emplace_back(tick, now);
    if (_inputs.size() > MAX_INPUTS)
      _inputs.pop_front();
  }

  /**
   * @brief Records a Snapshot message.
   * @param snapshot Decoded snapshot.
   * @param now Current time.
   */
  void onSnapshot(const protocol::SnapshotPayload &snapshot, double now) {
    _entities.emplace_back(now, static_cast<double>(snapshot.entities.size()));
    trim(_entities, now);

    const std::uint32_t tick = snapshot.serverTick.get();
    if (_hasFrame && tick == _lastTick)
      return;

    if (_hasFrame && isAfter(tick, _lastTick)) {
      _updateWait.emplace_back(now, (now - _lastFrameAt) * 1000.0);
      trim(_updateWait, now);
    }
    _hasFrame = true;
    _lastTick = tick;
    _lastFrameAt = now;
    _frames.emplace_back(now, tick);
    while (!_frames.empty() && now - _frames.front().first > WINDOW_SECONDS)
      _frames.pop_front();

    const std::uint32_t acked = snapshot.ackedInputTick.get();
    if (acked == 0)
      return;
    const std::pair<std::uint32_t, double> *match = nullptr;
    for (auto it = _inputs.rbegin(); it != _inputs.rend(); ++it) {
      if (!isAfter(it->first, acked)) {
        match = &*it;
        break;
      }
    }
    if (match != nullptr) {
      _confirmed.emplace_back(now, (now - match->second) * 1000.0);
      trim(_confirmed, now);
    }
  }

  /**
   * @brief Records the distance between the predicted ship and the server.
   * @param pixels Distance, in pixels.
   * @param now Current time.
   */
  void onPredictionGap(float pixels, double now) {
    _gap.emplace_back(now, static_cast<double>(pixels));
    trim(_gap, now);
  }

  /**
   * @brief Records an entity that a snapshot lists before its creation.
   * @param networkId Network id of the entity.
   * @param now Current time.
   */
  void onUnknownEntity(std::uint32_t networkId, double now) {
    _unknown.emplace(networkId, now);
  }

  /**
   * @brief Records the creation of an entity.
   * @param networkId Network id of the entity.
   * @param now Current time.
   */
  void onEntityCreated(std::uint32_t networkId, double now) {
    const auto waiting = _unknown.find(networkId);
    if (waiting == _unknown.end())
      return;
    _repair.emplace_back(now, (now - waiting->second) * 1000.0);
    trim(_repair, now);
    _unknown.erase(waiting);
  }

  /**
   * @brief Builds the report of the last seconds.
   * @param now Current time.
   * @return The summaries and rates.
   */
  Report report(double now) const {
    Report result;
    result.hasSnapshots = _hasFrame;
    result.updateWaitMs = summarize(_updateWait, now);
    result.confirmedInputMs = summarize(_confirmed, now);
    result.predictionGapPx = summarize(_gap, now);
    result.creationRepairMs = summarize(_repair, now);
    result.pendingCreations = _unknown.size();

    std::size_t recent = 0;
    std::uint32_t first = 0;
    std::uint32_t last = 0;
    bool any = false;
    for (const auto &[time, tick] : _frames) {
      if (now - time > WINDOW_SECONDS)
        continue;
      if (now - time <= RATE_SECONDS)
        ++recent;
      if (!any || isAfter(first, tick))
        first = tick;
      if (!any || isAfter(tick, last))
        last = tick;
      any = true;
    }
    result.snapshotsPerSecond = static_cast<double>(recent) / RATE_SECONDS;
    if (any) {
      const double expected = static_cast<double>(last - first) / TICKS_PER_FRAME + 1.0;
      std::size_t inWindow = 0;
      for (const auto &[time, tick] : _frames) {
        (void)tick;
        if (now - time <= WINDOW_SECONDS)
          ++inWindow;
      }
      result.updatesReceivedPercent =
          std::min(100.0, 100.0 * static_cast<double>(inWindow) / expected);
    }

    double entities = 0.0;
    std::size_t count = 0;
    for (const auto &[time, value] : _entities) {
      if (now - time > WINDOW_SECONDS)
        continue;
      entities += value;
      ++count;
    }
    result.entitiesPerSnapshot = count ? entities / static_cast<double>(count) : 0.0;
    return result;
  }

private:
  using Samples = std::deque<std::pair<double, double>>;

  static constexpr std::size_t MAX_INPUTS = 600;
  static constexpr double RATE_SECONDS = 2.0;
  static constexpr double TICKS_PER_FRAME = 3.0;

  static bool isAfter(std::uint32_t a, std::uint32_t b) {
    return static_cast<std::int32_t>(a - b) > 0;
  }

  static void trim(Samples &samples, double now) {
    while (!samples.empty() && now - samples.front().first > WINDOW_SECONDS)
      samples.pop_front();
  }

  static Summary summarize(const Samples &samples, double now) {
    Summary summary;
    std::vector<double> values;
    for (const auto &[time, value] : samples) {
      if (now - time <= WINDOW_SECONDS)
        values.push_back(value);
    }
    if (values.empty())
      return summary;
    summary.last = samples.back().second;
    summary.count = values.size();
    std::sort(values.begin(), values.end());
    const auto at = [&](double quantile) {
      return values[std::min(values.size() - 1,
                             static_cast<std::size_t>(quantile * static_cast<double>(values.size() - 1) + 0.5))];
    };
    summary.p50 = at(0.50);
    summary.p95 = at(0.95);
    summary.p99 = at(0.99);
    summary.max = values.back();
    return summary;
  }

  std::deque<std::pair<double, std::uint32_t>> _frames;
  std::deque<std::pair<std::uint32_t, double>> _inputs;
  Samples _updateWait;
  Samples _confirmed;
  Samples _gap;
  Samples _repair;
  Samples _entities;
  std::unordered_map<std::uint32_t, double> _unknown;
  bool _hasFrame = false;
  std::uint32_t _lastTick = 0;
  double _lastFrameAt = 0.0;
};

/** @} */
}
