#pragma once

#include <iostream>
#include <optional>

#include "EngineContext.hpp"
#include "events/NetworkEvent.hpp"
#include "protocol/HandshakeHeader.hpp"
#include "protocol/PacketWriter.hpp"

#include "scene/sceneType.hpp"

namespace rtype::client {
/**
 * @addtogroup api_client
 * @{
 */

/**
 * @brief Connection screen, which sends Hello every second until the server
 * answers or 10 seconds pass.
 */
class NetworkConnectionScene {
private:
  float _timeoutTimer = 0.0f;
  float _retryTimer = 0.0f;
  bool _isConnected = false;

public:
  /// Identifier of this scene.
  static constexpr SceneType Type = SceneType::Connection;

  /**
   * @brief Creates the scene.
   */
  NetworkConnectionScene(rtk::RenderWindow &) {}

  /**
   * @brief Subscribes to the connection result events.
   * @param context Engine services.
   */
  void onEnter(rtk::EngineContext &context) {
    context.eventBus.subscribe<rtype::events::ConnectedToServerEvent>(
        [this](const rtype::events::ConnectedToServerEvent &) {
          LOG_INFO("Handshake success connection");
          _isConnected = true;
        });

    context.eventBus.subscribe<rtype::events::ConnectionRejectedEvent>(
        [](const rtype::events::ConnectionRejectedEvent &) {
          LOG_ERROR("Connection refused by the server");
        });
  }

  /**
   * @brief Retries the handshake and picks the next scene.
   * @param context Engine services.
   * @param dt Elapsed time since the last frame, in seconds.
   * @return Game once connected, Menu after the timeout, else std::nullopt.
   */
  std::optional<SceneType> onUpdate(rtk::EngineContext &context,
                                    const rtk::InputState &, float dt) {
    if (_isConnected) {
      return SceneType::Game;
    }

    _retryTimer += dt;
    if (_retryTimer >= 1.0f) {
      _retryTimer = 0.0f;

      PacketWriter writer;
      HandshakeHeader helloHeader;
      helloHeader.type = HandshakeType::Hello;

      if (helloHeader.serialize(writer)) {
        LOG_INFO("Send handshake hello");

        context.eventBus.publish(
            rtype::events::SendPacketEvent{writer.getData()});
      }
    }

    _timeoutTimer += dt;
    if (_timeoutTimer >= 10.0f) {
      LOG_ERROR("Timeout: cannot join the server.");
      return SceneType::Menu;
    }

    return std::nullopt;
  }

  /**
   * @brief Does nothing.
   */
  void onExit(rtk::EngineContext &) {}
};

/** @} */
} // namespace rtype::client
