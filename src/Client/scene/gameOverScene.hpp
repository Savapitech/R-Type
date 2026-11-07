#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "EngineContext.hpp"
#include "font/font.hpp"
#include "graphical/renderWindow.hpp"
#include "scene/clientDependency.hpp"
#include "scene/sceneType.hpp"
#include "sceneManager/sceneConcept.hpp"

namespace rtype::client {
/**
 * @addtogroup api_client
 * @{
 */

/**
 * @brief Menu items available on the Game Over screen.
 */
enum class GameOverOption {
  ReturnToLobby = 0, ///< Return to current room lobby to ready up again.
  RoomList = 1,      ///< Leave current room and go to room browser.
  MainMenu = 2,      ///< Leave current room and return to main menu.
  Count = 3          ///< Number of selectable options.
};

/**
 * @brief Game Over scene displayed when player dies or match ends.
 *
 * Shows classic retro arcade game over styling with parallax space background,
 * mission status, and options to return to lobby, room list, or main menu.
 * Uses an input cooldown clock to prevent fast key skipping.
 */
class GameOverScene {
public:
  /**
   * @brief Constructs the Game Over scene.
   * @param dep Shared client dependencies.
   */
  explicit GameOverScene(clientDependancy &dep);

  /**
   * @brief Default destructor.
   */
  ~GameOverScene() = default;

  /// Identifier of this scene.
  static constexpr SceneType Type = SceneType::GameOver;

  /**
   * @brief Spawns the parallax background, header texts, and menu options.
   * @param context Engine services.
   */
  void onEnter(rtk::EngineContext &context);

  /**
   * @brief Handles navigation and transitions to other scenes with input clock cooldown.
   * @param context Engine services.
   * @param inputState Input state of the frame.
   * @param dt Elapsed time since the last frame, in seconds.
   * @return The next scene, or std::nullopt to stay.
   */
  std::optional<SceneType> onUpdate(rtk::EngineContext &context,
                                    const rtk::InputState &inputState,
                                    float dt);

  /**
   * @brief Cleans up entities spawned by this scene.
   * @param context Engine services.
   */
  void onExit(rtk::EngineContext &context);

private:
  void createBackground(rtk::EngineContext &context);
  void updateVisuals(rtk::EngineContext &context);
  void clearDynamicEntities(rtk::EngineContext &context);

  clientDependancy &_dependency;
  rtk::Font _font{};
  std::vector<std::size_t> _staticEntities;
  std::vector<std::size_t> _dynamicEntities;

  int _selectedOption = 0;
  float _elapsedTime = 0.0f;
  float _inputCooldown = 0.0f;
  bool _previousUp = false;
  bool _previousDown = false;
  bool _previousConfirm = false;
  bool _previousEscape = false;
  bool _firstFrame = true;
};

/** @} */
} // namespace rtype::client
