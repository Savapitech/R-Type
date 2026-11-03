#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "EngineContext.hpp"
#include "NetworkClient.hpp"
#include "graphical/renderWindow.hpp"
#include "scene/clientDependency.hpp"
#include "sceneManager/sceneConcept.hpp"
#include "sceneType.hpp"

namespace rtype::client {
/**
 * @addtogroup api_client
 * @{
 */

class Game;

/**
 * @brief Scene with the list of all the rooms available.
 */
class RoomListScene {
public:
  /**
   * @brief Creates the scene.
   * @param window Window used to load textures.
   */
  explicit RoomListScene(clientDependancy &dep);
  ~RoomListScene() = default;

  /// Identifier of this scene.
  static constexpr SceneType Type = SceneType::RoomList;

  /**
   * @brief Spawns the menu logo and the PLAY, SETTING, and QUIT sprites.
   * @param context Engine services.
   */
  void onEnter(rtk::EngineContext &context);

  /**
   * @brief Navigates between buttons and triggers the selected action.
   * @param context Engine services.
   * @param inputState Input state of the frame.
   * @param dt Elapsed time since the last frame, in seconds.
   * @return The next scene, or std::nullopt to stay.
   */
  std::optional<SceneType> onUpdate(rtk::EngineContext &context,
                                    const rtk::InputState &inputState,
                                    float dt);

  /**
   * @brief Destroys the menu entities.
   * @param context Engine services.
   */
  void onExit(rtk::EngineContext &context);

private:
  void updateSelectionVisuals(rtk::EngineContext &context);
  clientDependancy &_dependency;
  std::vector<std::size_t> _entities;
  std::uint32_t _logoTextureId = 0;
  std::uint32_t _backTextureId = 0;
  std::uint32_t _roomTextureId = 0;
  std::uint32_t _refreshTextureId = 0;
  std::uint32_t _createRoomTextureId = 0;
  std::size_t _refreshEntity = 0;
  std::size_t _createRoomEntity = 0;
  int _selectedOption = 0;
  uint32_t _previousActionMask = 0;
};

/** @} */
} // namespace rtype::client
