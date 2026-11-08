#pragma once

#include <ISystem.hpp>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "ClientMetrics.hpp"
#include "LocalPrediction.hpp"
#include "Logger/Logger.hpp"
#include "NetworkDiagnostics.hpp"
#include "components/Transform.hpp"
#include "graphical/renderWindow.hpp"
#include "inputAction/InputAction.hpp"
#include "systems/RenderConcept.hpp"

namespace rtk::systems {
/**
 * @addtogroup api_client
 * @{
 */

/**
 * @brief Shows the network metrics on screen while the debug key (F3) is on.
 *
 * Like the debug screen of a sandbox game, it lists what the network does and
 * what the player feels of it: round trip time, packet loss, bandwidth, wait
 * between two world updates, delay before the server confirms an input, gap
 * of the predicted ship. The complete list, with the extra counters, is written
 * on the console when the key is pressed and then every few seconds.
 */
class NetworkHudSystem final : public rtk::ecs::ISystem {
public:
  /**
   * @brief Creates the metrics HUD.
   * @param window Target render window used to load textures and fonts.
   * @param inputState Input state holding the debug toggle.
   * @param stats Counters of the network layer, updated by the game.
   * @param diagnostics What the player feels, measured by the game.
   * @param prediction Prediction of the ship of the local player.
   * @param font Font used to render text strings.
   */
  NetworkHudSystem(rtk::RenderWindow &window, const rtk::InputState &inputState,
                   const rtype::client::ClientNetworkStats &stats,
                   const rtype::client::NetworkDiagnostics &diagnostics,
                   const rtype::client::LocalPrediction &prediction,
                   rtk::Font font = {})
      : _window(window), _inputState(inputState), _stats(stats),
        _diagnostics(diagnostics), _prediction(prediction), _font(font) {}

  ~NetworkHudSystem() override = default;

  /**
   * @brief Loads the font.
   */
  void onStart(rtk::ecs::Registry &) override {
    LOG_INFO("Load Network HUD System");
    if (!_font.isValid()) {
      _font = _window.loadFont("./assets/font/r-type.otf", 40.0f);
    }
    ensureWhiteTexture();
  }

  /**
   * @brief Shows, refreshes or hides the metrics, and writes the console report.
   * @param reg Entity registry.
   * @param dt Elapsed time since last frame in seconds.
   */
  void update(rtk::ecs::Registry &reg, float dt) override {
    const bool shown = _inputState.debug;
    if (shown && !_lastDisplayed) {
      printConsoleReport();
      _consoleTimer = 0.0f;
    }

    _textUpdateTimer += dt;
    if (shown) {
      _consoleTimer += dt;
      if (_consoleTimer >= CONSOLE_PERIOD) {
        printConsoleReport();
        _consoleTimer = 0.0f;
      }
    }

    if (shown != _lastDisplayed) {
      _lastDisplayed = shown;
      updateTextEntities(reg);
      _textUpdateTimer = 0.0f;
    } else if (shown && _textUpdateTimer >= UPDATE_INTERVAL) {
      updateTextEntities(reg);
      _textUpdateTimer = 0.0f;
    }
  }

  /**
   * @brief Destroys the HUD entities when the system stops.
   * @param reg Entity registry.
   */
  void onStop(rtk::ecs::Registry &reg) override { clearTextEntities(reg); }

  /**
   * @brief Updates the active font handle.
   * @param font Valid font handle.
   */
  void setFont(rtk::Font font) noexcept { _font = font; }

private:
  static constexpr float PANEL_X = 200.0f;
  static constexpr float PANEL_Y = 8.0f;
  static constexpr float PANEL_WIDTH = 640.0f;
  static constexpr float LINE_HEIGHT = 19.0f;
  static constexpr float UPDATE_INTERVAL = 0.25f;
  static constexpr float CONSOLE_PERIOD = 5.0f;

  enum class Level { Good, Warning, Bad };

  struct Line {
    std::string text;
    Level level;
  };

  static double nowSeconds() {
    return std::chrono::duration<double>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
  }

  static const char *tag(Level level) {
    switch (level) {
    case Level::Good:
      return "[OK]";
    case Level::Warning:
      return "[WARN]";
    default:
      return "[BAD]";
    }
  }

  static Level lower(double value, double good, double warning) {
    if (value <= good)
      return Level::Good;
    return value <= warning ? Level::Warning : Level::Bad;
  }

  static Level higher(double value, double good, double warning) {
    if (value >= good)
      return Level::Good;
    return value >= warning ? Level::Warning : Level::Bad;
  }

  static const char *verdict(const rtype::client::ClientNetworkStats &stats,
                             const rtype::client::NetworkDiagnostics::Report &report,
                             Level &level) {
    if (!report.hasSnapshots) {
      level = Level::Warning;
      return "WAITING FOR THE GAME";
    }
    const double rtt = stats.smoothPingMs;
    const double wait = report.updateWaitMs.p99;
    const double received = report.updatesReceivedPercent;
    if (wait <= 100.0 && received >= 95.0 && rtt <= 100.0) {
      level = Level::Good;
      return "GOOD";
    }
    if (wait <= 200.0 && received >= 85.0 && rtt <= 250.0) {
      level = Level::Warning;
      return "PLAYABLE";
    }
    level = Level::Bad;
    return "DEGRADED";
  }

  std::vector<Line> buildLines() const {
    const double now = nowSeconds();
    const auto report = _diagnostics.report(now);
    const auto &prediction = _prediction.stats();
    char buffer[160];
    std::vector<Line> lines;

    Level overall = Level::Good;
    const char *state = verdict(_stats, report, overall);
    std::snprintf(buffer, sizeof(buffer), "NETWORK  %s", state);
    lines.push_back({buffer, overall});

    std::snprintf(buffer, sizeof(buffer), "RTT       %.0f ms  last %.0f  jitter %.1f  %s",
                  _stats.smoothPingMs, _stats.pingMs, _stats.jitterMs,
                  tag(lower(_stats.smoothPingMs, 100.0, 250.0)));
    lines.push_back({buffer, lower(_stats.smoothPingMs, 100.0, 250.0)});

    std::snprintf(buffer, sizeof(buffer), "LOSS      %.1f %%  resent %.1f/s  dup %llu  reordered %llu  %s",
                  _stats.packetLossPercent, _stats.retransmissionsPerSecond,
                  static_cast<unsigned long long>(_stats.totalDuplicates),
                  static_cast<unsigned long long>(_stats.totalOutOfOrder),
                  tag(lower(_stats.packetLossPercent, 2.0, 10.0)));
    lines.push_back({buffer, lower(_stats.packetLossPercent, 2.0, 10.0)});

    const double bytesPerPacket =
        _stats.packetsInPerSecond > 0.0
            ? _stats.downloadKiBs * 1024.0 / _stats.packetsInPerSecond
            : 0.0;
    std::snprintf(buffer, sizeof(buffer), "DOWN      %.2f KB/s  %.0f pkt/s  %.0f B/pkt",
                  _stats.downloadKiBs, _stats.packetsInPerSecond, bytesPerPacket);
    lines.push_back({buffer, Level::Good});

    std::snprintf(buffer, sizeof(buffer), "UP        %.2f KB/s  %.0f pkt/s",
                  _stats.uploadKiBs, _stats.packetsOutPerSecond);
    lines.push_back({buffer, Level::Good});

    const auto waitLevel = lower(report.updateWaitMs.p99, 100.0, 200.0);
    std::snprintf(buffer, sizeof(buffer), "WORLD     %.1f updates/s  wait p99 %.0f  max %.0f ms  %s",
                  report.snapshotsPerSecond, report.updateWaitMs.p99,
                  report.updateWaitMs.max, tag(waitLevel));
    lines.push_back({buffer, waitLevel});

    const auto receivedLevel = higher(report.updatesReceivedPercent, 95.0, 85.0);
    std::snprintf(buffer, sizeof(buffer), "UPDATES   %.0f %% of 20 Hz  %.1f entities each  %s",
                  report.updatesReceivedPercent, report.entitiesPerSnapshot,
                  tag(receivedLevel));
    lines.push_back({buffer, receivedLevel});

    const auto inputLevel = lower(report.confirmedInputMs.p50, 100.0, 250.0);
    std::snprintf(buffer, sizeof(buffer), "INPUT     server confirms in %.0f ms (p95 %.0f)  %s",
                  report.confirmedInputMs.p50, report.confirmedInputMs.p95,
                  tag(inputLevel));
    lines.push_back({buffer, inputLevel});

    std::snprintf(buffer, sizeof(buffer), "SHIP      %s moves at once (prediction)",
                  _prediction.isActive() ? "seen:" : "no prediction, seen: waits for server, ");
    lines.push_back({buffer, _prediction.isActive() ? Level::Good : Level::Warning});

    const auto gapLevel = lower(report.predictionGapPx.p95, 10.0, 40.0);
    std::snprintf(buffer, sizeof(buffer), "PREDICT   gap p95 %.1f px  max %.1f  fixed %llu  jumped %llu  %s",
                  report.predictionGapPx.p95, report.predictionGapPx.max,
                  static_cast<unsigned long long>(prediction.corrections),
                  static_cast<unsigned long long>(prediction.snaps), tag(gapLevel));
    lines.push_back({buffer, gapLevel});

    const auto repairLevel = lower(report.creationRepairMs.p95, 300.0, 1000.0);
    std::snprintf(buffer, sizeof(buffer), "CREATIONS repaired in p95 %.0f ms  max %.0f  waiting %zu  %s",
                  report.creationRepairMs.p95, report.creationRepairMs.max,
                  report.pendingCreations, tag(repairLevel));
    lines.push_back({buffer, repairLevel});

    std::snprintf(buffer, sizeof(buffer), "TOTAL     down %.1f KB (%llu pkt)  up %.1f KB (%llu pkt)",
                  static_cast<double>(_stats.totalBytesReceived) / 1024.0,
                  static_cast<unsigned long long>(_stats.totalPacketsReceived),
                  static_cast<double>(_stats.totalBytesSent) / 1024.0,
                  static_cast<unsigned long long>(_stats.totalPacketsSent));
    lines.push_back({buffer, Level::Good});
    return lines;
  }

  void printConsoleReport() const {
    const double now = nowSeconds();
    const auto report = _diagnostics.report(now);
    const auto &prediction = _prediction.stats();
    Level overall = Level::Good;
    const char *state = verdict(_stats, report, overall);
    char buffer[256];
    std::cout << "Network metrics\n";
    std::cout << "State                : " << state << "\n";
    std::snprintf(buffer, sizeof(buffer),
                  "RTT                  : last %.1f ms, smooth %.1f ms, jitter %.2f ms",
                  _stats.pingMs, _stats.smoothPingMs, _stats.jitterMs);
    std::cout << buffer << "\n";
    std::snprintf(buffer, sizeof(buffer),
                  "Packet loss          : %.2f %% (%llu lost)  resent %.2f/s (%llu total)  duplicated %llu  reordered %llu",
                  _stats.packetLossPercent,
                  static_cast<unsigned long long>(_stats.totalPacketsLost),
                  _stats.retransmissionsPerSecond,
                  static_cast<unsigned long long>(_stats.totalRetransmissions),
                  static_cast<unsigned long long>(_stats.totalDuplicates),
                  static_cast<unsigned long long>(_stats.totalOutOfOrder));
    std::cout << buffer << "\n";
    std::snprintf(buffer, sizeof(buffer),
                  "Bandwidth down       : %.3f KB/s, %.1f packets/s",
                  _stats.downloadKiBs, _stats.packetsInPerSecond);
    std::cout << buffer << "\n";
    std::snprintf(buffer, sizeof(buffer),
                  "Bandwidth up         : %.3f KB/s, %.1f packets/s",
                  _stats.uploadKiBs, _stats.packetsOutPerSecond);
    std::cout << buffer << "\n";
    std::snprintf(buffer, sizeof(buffer),
                  "World updates        : %.2f/s, %.0f %% of the normal 20 Hz received, %.1f entities each",
                  report.snapshotsPerSecond, report.updatesReceivedPercent,
                  report.entitiesPerSnapshot);
    std::cout << buffer << "\n";
    std::snprintf(buffer, sizeof(buffer),
                  "Wait between updates : last %.0f, p50 %.0f, p95 %.0f, p99 %.0f, max %.0f ms (%zu samples, normal 50)",
                  report.updateWaitMs.last, report.updateWaitMs.p50,
                  report.updateWaitMs.p95, report.updateWaitMs.p99,
                  report.updateWaitMs.max, report.updateWaitMs.count);
    std::cout << buffer << "\n";
    std::snprintf(buffer, sizeof(buffer),
                  "Input confirmed in   : last %.0f, p50 %.0f, p95 %.0f, max %.0f ms (%zu samples)",
                  report.confirmedInputMs.last, report.confirmedInputMs.p50,
                  report.confirmedInputMs.p95, report.confirmedInputMs.max,
                  report.confirmedInputMs.count);
    std::cout << buffer << "\n";
    std::cout << "Input seen on screen : "
              << (_prediction.isActive()
                      ? "same frame (prediction of the local ship)"
                      : "after the server confirmation (no prediction)")
              << "\n";
    std::snprintf(buffer, sizeof(buffer),
                  "Prediction gap       : last %.2f, p50 %.2f, p95 %.2f, max %.2f px; %llu checks, %llu smooth fixes, %llu jumps",
                  report.predictionGapPx.last, report.predictionGapPx.p50,
                  report.predictionGapPx.p95, report.predictionGapPx.max,
                  static_cast<unsigned long long>(prediction.reconciliations),
                  static_cast<unsigned long long>(prediction.corrections),
                  static_cast<unsigned long long>(prediction.snaps));
    std::cout << buffer << "\n";
    std::snprintf(buffer, sizeof(buffer),
                  "Creation repair      : p50 %.0f, p95 %.0f, max %.0f ms (%zu samples), %zu entities waiting",
                  report.creationRepairMs.p50, report.creationRepairMs.p95,
                  report.creationRepairMs.max, report.creationRepairMs.count,
                  report.pendingCreations);
    std::cout << buffer << "\n";
    std::snprintf(buffer, sizeof(buffer),
                  "Totals               : down %llu B in %llu packets, up %llu B in %llu packets",
                  static_cast<unsigned long long>(_stats.totalBytesReceived),
                  static_cast<unsigned long long>(_stats.totalPacketsReceived),
                  static_cast<unsigned long long>(_stats.totalBytesSent),
                  static_cast<unsigned long long>(_stats.totalPacketsSent));
    std::cout << buffer << "\n";
    std::cout << std::flush;
  }

  void updateTextEntities(rtk::ecs::Registry &reg) {
    clearTextEntities(reg);

    if (!_inputState.debug)
      return;

    const auto lines = buildLines();
    addSolidRect(reg, PANEL_X - 8.0f, PANEL_Y - 4.0f, PANEL_WIDTH,
                 static_cast<float>(lines.size()) * LINE_HEIGHT + 12.0f,
                 {0, 0, 0, 175});

    float y = PANEL_Y + 14.0f;
    for (const auto &line : lines) {
      addText(reg, line.text, PANEL_X, y, colorOf(line.level), 0.34f);
      y += LINE_HEIGHT;
    }
  }

  static rtk::ecs::concepts::ColorRGBA8 colorOf(Level level) {
    switch (level) {
    case Level::Good:
      return {80, 255, 120, 255};
    case Level::Warning:
      return {255, 200, 40, 255};
    default:
      return {255, 90, 90, 255};
    }
  }

  void addText(rtk::ecs::Registry &reg, const std::string &content, float x,
               float y, const rtk::ecs::concepts::ColorRGBA8 &color,
               float scaleFactor = 0.45f) {
    if (!_font.isValid()) {
      _font = _window.loadFont("./assets/font/r-type.otf", 40.0f);
    }
    if (!_font.isValid() || content.empty())
      return;

    const rtk::Text text =
        _window.createText(content, _font, {x, y},
                           rtk::ColorRGBA8{color.r, color.g, color.b, color.a});

    for (const auto &glyph : text.getSprites()) {
      const auto entity = reg.spawn_entity();

      rtk::ecs::concepts::SpriteData sprite{};
      sprite.textureId = glyph.textureId;
      sprite.textureRect.x = glyph.textureRect.left;
      sprite.textureRect.y = glyph.textureRect.top;
      sprite.textureRect.width = glyph.textureRect.width;
      sprite.textureRect.height = glyph.textureRect.height;
      sprite.color = {glyph.color.r, glyph.color.g, glyph.color.b,
                      glyph.color.a};
      sprite.size = {glyph.size.x, glyph.size.y};
      sprite.origin = {glyph.origin.x, glyph.origin.y};
      sprite.scale = {glyph.scale.x * scaleFactor, glyph.scale.y * scaleFactor};
      sprite.layer = 102;
      sprite.flags = 0;

      ::component::Transform transform{};
      transform.position.x = x + (glyph.position.x - x) * scaleFactor;
      transform.position.y = y + (glyph.position.y - y) * scaleFactor;
      transform.scale = {glyph.scale.x * scaleFactor,
                         glyph.scale.y * scaleFactor};
      transform.rotation = 0.0f;

      reg.get_components<::component::Transform>().insert_at(entity, transform);
      reg.get_components<rtk::ecs::concepts::SpriteData>().insert_at(entity,
                                                                     sprite);

      _textEntities.push_back(entity);
    }
  }

  void ensureWhiteTexture() {
    if (!_whiteTexture.isValid()) {
      static const std::uint8_t whitePixel[4] = {255, 255, 255, 255};
      _whiteTexture = _window.loadTextureFromMemory(
          std::span<const std::uint8_t>(whitePixel, 4), 1, 1);
      if (!_whiteTexture.isValid()) {
        _whiteTexture =
            _window.loadTexture("assets/entityTexture/Background/withe.jpg");
      }
    }
  }

  void addSolidRect(rtk::ecs::Registry &reg, float x, float y, float width,
                    float height, const rtk::ecs::concepts::ColorRGBA8 &color) {
    if (width <= 0.0f || height <= 0.0f)
      return;
    ensureWhiteTexture();
    if (!_whiteTexture.isValid())
      return;

    const auto entity = reg.spawn_entity();

    component::Transform transform{};
    transform.position = {x, y};
    transform.scale = {1.0f, 1.0f};
    transform.rotation = 0.0f;

    rtk::ecs::concepts::SpriteData sprite{};
    sprite.textureId = _whiteTexture.getHandle();
    sprite.size = {width, height};
    sprite.origin = {0.0f, 0.0f};
    sprite.textureRect = {0, 0, 1, 1};
    sprite.color = color;
    sprite.scale = {1.0f, 1.0f};
    sprite.layer = 101;
    sprite.flags = 0;

    reg.get_components<component::Transform>().insert_at(entity, transform);
    reg.get_components<rtk::ecs::concepts::SpriteData>().insert_at(entity,
                                                                   sprite);
    _textEntities.push_back(entity);
  }

  void clearTextEntities(rtk::ecs::Registry &reg) {
    for (auto ent : _textEntities) {
      reg.remove_entity_from_all_pools(ent);
      reg.kill_entity(ent);
    }
    _textEntities.clear();
  }

  rtk::RenderWindow &_window;
  const rtk::InputState &_inputState;
  const rtype::client::ClientNetworkStats &_stats;
  const rtype::client::NetworkDiagnostics &_diagnostics;
  const rtype::client::LocalPrediction &_prediction;
  rtk::Font _font{};
  std::vector<std::size_t> _textEntities{};

  bool _lastDisplayed = false;
  float _textUpdateTimer = 0.0f;
  float _consoleTimer = 0.0f;
  rtk::Texture _whiteTexture{};
};

/** @} */
}
