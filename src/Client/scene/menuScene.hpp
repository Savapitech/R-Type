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
 * @brief Menu items available on the title screen.
 */
enum class MenuOption {
  Play = 0, ///< Launch the game (connect to server).
  Setting,  ///< Return to the intro scene.
  Quit,     ///< Quit the application.
  Count     ///< Number of options.
};

/**
 * @brief Title screen with PLAY, SETTING, and QUIT options.
 */
class MenuScene {
public:
  /**
   * @brief Creates the scene.
   * @param window Window used to load textures.
   */
  explicit MenuScene(rtk::RenderWindow &window);
  ~MenuScene() = default;

  /// Identifier of this scene.
  static constexpr SceneType Type = SceneType::Menu;

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
  /**
   * @brief Updates colors, scales, and positions of the menu buttons.
   * @param context Engine services.
   */
  void updateSelectionVisuals(rtk::EngineContext &context);

  /**
   * @brief Animates the menu buttons sliding down from the top.
   * @param context Engine services.
   * @param time Elapsed transition time in seconds.
   */
  void updateTransitionVisuals(rtk::EngineContext &context, float time);

  rtk::RenderWindow &_window;

  std::vector<std::size_t> _entities;
  std::uint32_t _logoTextureId = 0;
  std::uint32_t _playTextureId = 0;
  std::uint32_t _settingTextureId = 0;
  std::uint32_t _quitTextureId = 0;

  std::size_t _playEntity = 0;
  std::size_t _settingEntity = 0;
  std::size_t _quitEntity = 0;

  int _selectedOption = 0;
  uint32_t _previousActionMask = 0;

  float _transitionTime = 0.0f;
  bool _transitionFinished = false;
};

/** @} */
} // namespace rtype::client
