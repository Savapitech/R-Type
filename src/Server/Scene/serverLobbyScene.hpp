#pragma once

#include "sceneManager/sceneConcept.hpp"
#include "serverScene.hpp"
#include <optional>

namespace rtype::server {
/**
 * @addtogroup api_server
 * @{
 */

/**
 * @brief Waiting room: starts the round with 2 players, or after 30 seconds
 * with 1 player.
 */
class ServerLobbyScene {
public:
  /// Identifier of this scene.
  static constexpr ServerSceneType Type = ServerSceneType::Lobby;

  /**
   * @brief Creates the scene.
   * @param dep Game that owns the scene.
   */
  ServerLobbyScene(ServerDependency &dep);

  /**
   * @brief Resets the lobby timer.
   * @param context Engine services.
   */
  void onEnter(rtk::EngineContext &context);

  /**
   * @brief Processes network events and starts the round when the conditions
   * are met.
   * @param context Engine services.
   * @param input Unused.
   * @param dt Elapsed time since the last frame, in seconds.
   * @return The next scene, or std::nullopt to stay.
   */
  std::optional<ServerSceneType> onUpdate(rtk::EngineContext &context,
                                          const ServerInput &input, float dt);

  /**
   * @brief Does nothing.
   * @param context Engine services.
   */
  void onExit(rtk::EngineContext &context);

private:
  ServerDependency &_dependency;
  float _lobbyTimer = 0.0f;
  int _lastPrintedCountdown = -1;
};

/** @} */
} // namespace rtype::server
