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
 * @brief Round in progress: simulates the stage and returns to the lobby when
 * every player left.
 */
class ServerGameScene {
public:
  /// Identifier of this scene.
  static constexpr ServerSceneType Type = ServerSceneType::InGame;

  /**
   * @brief Creates the scene.
   * @param dep Game that owns the scene.
   */
  ServerGameScene(ServerDependency &dep);

  /**
   * @brief Builds the stage and sends every entity to the players.
   * @param context Engine services.
   */
  void onEnter(rtk::EngineContext &context);

  /**
   * @brief Processes network events, advances the tick and sends a snapshot
   * every 3 ticks.
   * @param context Engine services.
   * @param input Unused.
   * @param dt Elapsed time since the last frame, in seconds.
   * @return The next scene, or std::nullopt to stay.
   */
  std::optional<ServerSceneType> onUpdate(rtk::EngineContext &context,
                                          const ServerInput &input, float dt);

  /**
   * @brief Destroys every entity and resets the game state.
   * @param context Engine services.
   */
  void onExit(rtk::EngineContext &context);

private:
  ServerDependency &_dependency;
};

/** @} */
} // namespace rtype::server
