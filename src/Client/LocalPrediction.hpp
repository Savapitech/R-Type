#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>

#include "Protocol/InputPayload.hpp"
#include "Protocol/ShipMovement.hpp"

namespace rtype::client {
/**
 * @addtogroup api_client
 * @{
 */

/**
 * @brief Client side prediction of the ship of the local player, with server
 * reconciliation.
 *
 * Without it the ship only moves when a Snapshot comes back, so it reacts to
 * a key one round trip time (plus up to one snapshot period) late. With it the
 * ship moves at once, using the same movement as the server
 * (protocol::shipVelocity), and every Snapshot corrects the prediction:
 *
 * - every frame, step() moves the predicted ship and records the step;
 * - every Snapshot, reconcile() takes the position of the server, which
 *   accounts for the inputs up to `ackedTick`, and replays the recorded steps
 *   that the server has not seen yet. That gives the position the ship should
 *   have now. The predicted ship is moved toward it: smoothly for small
 *   errors, at once for large ones (a hit, a lost packet).
 *
 * The class knows nothing about graphics or networking, the client and the
 * test bot use the same code.
 */
class LocalPrediction {
public:
  /// Errors below this many pixels are ignored (a position has a 0.25 pixel
  /// step).
  static constexpr float DEADZONE = 0.75f;
  /// Errors above this many pixels move the ship at once instead of smoothly.
  static constexpr float SNAP_DISTANCE = 96.0f;
  /// Share of the error removed at each Snapshot for a smooth correction.
  static constexpr float CORRECTION = 0.5f;
  /// Steps kept for the replay, a few seconds at any frame rate.
  static constexpr std::size_t MAX_HISTORY = 1024;

  /// What reconcile() did, for metrics.
  struct Stats {
    std::uint64_t reconciliations = 0; ///< Snapshots used.
    std::uint64_t corrections = 0;     ///< Smooth corrections applied.
    std::uint64_t snaps = 0;           ///< Corrections applied at once.
    float lastError = 0.0f;            ///< Distance to the target, pixels.
    float maxError = 0.0f;             ///< Largest distance seen, pixels.
  };

  /**
   * @brief Forgets the position, the history and the ship.
   */
  void reset() {
    _history.clear();
    _ownNetworkId = 0;
    _lastAckedTick = 0;
    _initialized = false;
    _x = _y = 0.0f;
    _stats = {};
  }

  /**
   * @brief Sets the network id of the ship of the local player.
   *
   * A different id (new game, new ship) starts the prediction again.
   *
   * @param networkId Id given by the server in every Snapshot, 0 if none.
   */
  void setOwnNetworkId(std::uint32_t networkId) {
    if (networkId == _ownNetworkId)
      return;
    const Stats stats = _stats;
    reset();
    _stats = stats;
    _ownNetworkId = networkId;
  }

  /// Network id of the ship of the local player, 0 if unknown.
  std::uint32_t ownNetworkId() const { return _ownNetworkId; }

  /// True once the ship has a position and can be moved.
  bool isActive() const { return _ownNetworkId != 0 && _initialized; }

  /**
   * @brief Moves the predicted ship for one frame.
   * @param dt Duration of the frame, in seconds.
   * @param actions Actions held during the frame.
   * @param speed Speed of the ship, in pixels per second.
   * @param tick Client input tick of the frame (NetworkClient::inputTick()).
   */
  void step(float dt, protocol::InputAction actions, float speed,
            std::uint32_t tick) {
    if (!isActive())
      return;
    const protocol::ShipVelocity velocity =
        protocol::shipVelocity(actions, speed);
    _x += velocity.x * dt;
    _y += velocity.y * dt;
    _history.push_back({tick, velocity.x * dt, velocity.y * dt});
    if (_history.size() > MAX_HISTORY)
      _history.pop_front();
  }

  /**
   * @brief Corrects the prediction with the state sent by the server.
   * @param serverX Horizontal position of the ship in the Snapshot.
   * @param serverY Vertical position of the ship in the Snapshot.
   * @param ackedTick Client tick the server state accounts for
   * (SnapshotPayload::ackedInputTick), 0 if the server has no input yet.
   */
  void reconcile(float serverX, float serverY, std::uint32_t ackedTick) {
    if (_ownNetworkId == 0)
      return;
    if (!_initialized) {
      _x = serverX;
      _y = serverY;
      _initialized = true;
      return;
    }

    if (ackedTick != 0) {
      // A Snapshot that arrives late describes the ship before the one already
      // used: using it would pull the ship backward.
      if (_lastAckedTick != 0 && isAfter(_lastAckedTick, ackedTick))
        return;
      _lastAckedTick = ackedTick;
      while (!_history.empty() && !isAfter(_history.front().tick, ackedTick))
        _history.pop_front();
    }

    float targetX = serverX;
    float targetY = serverY;
    for (const Step &step : _history) {
      targetX += step.dx;
      targetY += step.dy;
    }

    const float errorX = targetX - _x;
    const float errorY = targetY - _y;
    const float error = std::hypot(errorX, errorY);
    ++_stats.reconciliations;
    _stats.lastError = error;
    if (error > _stats.maxError)
      _stats.maxError = error;

    if (error > SNAP_DISTANCE) {
      _x = targetX;
      _y = targetY;
      ++_stats.snaps;
    } else if (error > DEADZONE) {
      _x += errorX * CORRECTION;
      _y += errorY * CORRECTION;
      ++_stats.corrections;
    }
  }

  /// Predicted horizontal position, in pixels.
  float x() const { return _x; }
  /// Predicted vertical position, in pixels.
  float y() const { return _y; }
  /// Counters of the reconciliation.
  const Stats &stats() const { return _stats; }

private:
  struct Step {
    std::uint32_t tick;
    float dx;
    float dy;
  };

  // True if tick `a` is after tick `b`, whatever the wrap around.
  static bool isAfter(std::uint32_t a, std::uint32_t b) {
    return static_cast<std::int32_t>(a - b) > 0;
  }

  std::deque<Step> _history;
  std::uint32_t _ownNetworkId = 0;
  std::uint32_t _lastAckedTick = 0;
  bool _initialized = false;
  float _x = 0.0f;
  float _y = 0.0f;
  Stats _stats;
};

/** @} */
} // namespace rtype::client
