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
 * @brief Intro scene with the r type logo.
 */
class IntroScene {
public:
  /**
   * @brief Creates the scene.
   * @param window Window used to load the intro textures.
   */
  explicit IntroScene(rtk::RenderWindow &window);
  ~IntroScene() = default;

  /// Identifier of this scene.
  static constexpr SceneType Type = SceneType::Intro;

  /**
   * @brief Creates the logo and the fade overlay.
   * @param context Engine services.
   */
  void onEnter(rtk::EngineContext &context);

  /**
   * @brief Runs the fade, then the logo animation once Confirm is pressed.
   * @param context Engine services.
   * @param inputState Current player input.
   * @param dt Elapsed time since the last frame, in seconds.
   * @return SceneType::Menu once the animation is over, nothing otherwise.
   */
  std::optional<SceneType> onUpdate(rtk::EngineContext &context,
                                    const rtk::InputState &inputState,
                                    float dt);

  /**
   * @brief Destroys the entities of the scene.
   * @param context Engine services.
   */
  void onExit(rtk::EngineContext &context);

private:
  rtk::RenderWindow &_window;

  std::vector<std::size_t> _entities;
  std::uint32_t _textureId = 0;

  std::size_t _fadeEntity;
  float _currentAlpha = 255.0f;

  std::size_t _logoEntity;
  bool _isTransitioning = false;
};

/** @} */
} // namespace rtype::client
