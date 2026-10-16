#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "graphical/renderWindow.hpp"
#include "sceneManager/sceneConcept.hpp"

#include "sceneType.hpp"

namespace rtype::client {
/**
 * @addtogroup api_client
 * @{
 */

/**
 * @brief Title screen, which waits for the Confirm action.
 */
class MenuScene {
public:
  /**
   * @brief Creates the scene.
   * @param window Window used to load the menu texture.
   */
  explicit MenuScene(rtk::RenderWindow &window);
  ~MenuScene() = default;

  /// Identifier of this scene.
  static constexpr SceneType Type = SceneType::Menu;

  /**
   * @brief Shows the menu picture.
   * @param context Engine services.
   */
  void onEnter(rtk::EngineContext &context);

  /**
   * @brief Goes to the connection scene when Confirm is pressed.
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
  rtk::RenderWindow &_window;

  std::vector<std::size_t> _entities;
  std::uint32_t _textureId = 0;
};

/** @} */
} // namespace rtype::client
