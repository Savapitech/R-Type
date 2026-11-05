#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "EngineContext.hpp"
#include "font/font.hpp"
#include "scene/clientDependency.hpp"
#include "scene/sceneType.hpp"
#include "sceneManager/sceneConcept.hpp"

namespace rtype::client {

/**
 * @brief Lobby scene displayed after joining or creating a room.
 *
 * Displays connected players and their ready states.
 * Allows the local player to toggle Ready and leave the room.
 * Waits for the server's GameStarting message before
 * transitioning to the game scene.
 */
class RoomLobbyScene {
public:
  /**
   * @brief Constructs a room lobby scene.
   * @param dep Shared client dependencies.
   */
  explicit RoomLobbyScene(clientDependancy &dep);

  /**
   * @brief Default destructor.
   */
  ~RoomLobbyScene() = default;

  /** @brief Identifier of the room lobby scene. */
  static constexpr SceneType Type = SceneType::RoomLobby;

  /**
   * @brief Initializes the lobby scene.
   * @param context Engine services.
   */
  void onEnter(rtk::EngineContext &context);

  /**
   * @brief Updates the lobby and processes player input.
   *
   * Handles Ready/Not Ready requests, leaving the room,
   * and the transition to GameScene after GameStarting.
   *
   * @param context Engine services.
   * @param inputState Current input state.
   * @param dt Elapsed time since the previous frame.
   * @return Next scene, or std::nullopt to remain in the lobby.
   */
  std::optional<SceneType> onUpdate(rtk::EngineContext &context,
                                    const rtk::InputState &inputState,
                                    float dt);

  /**
   * @brief Removes entities created by the lobby scene.
   * @param context Engine services.
   */
  void onExit(rtk::EngineContext &context);

private:
  /**
   * @brief Rebuilds the displayed player list and ready indicators.
   * @param context Engine services.
   */
  void updatePlayerDisplay(rtk::EngineContext &context);
  /**
   * @brief Creates decorative scrolling backgrounds for the lobby.
   *
   * These entities are client-side only and have no collision
   * or gameplay components.
   *
   * @param context Engine services.
   */
  void createPreviewBackground(rtk::EngineContext &context);
  clientDependancy &_dependency;
  std::vector<std::size_t> _entities;
  std::vector<std::size_t> _playerEntities;
  rtk::Font _font;
  std::uint32_t _previousActionMask = 0;
  bool _leaveRequested = false;
  std::string _displaySignature;
};

} // namespace rtype::client
