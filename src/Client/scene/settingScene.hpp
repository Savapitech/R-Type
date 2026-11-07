#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "font/font.hpp"
#include "graphical/renderWindow.hpp"
#include "scene/clientDependency.hpp"
#include "sceneManager/sceneConcept.hpp"
#include "sceneType.hpp"
#include "systems/RenderConcept.hpp"

namespace rtype::client {
/**
 * @addtogroup api_client
 * @{
 */

/**
 * @brief Configurable action binding description.
 */
struct BindingOption {
  std::string label; ///< Display text for the action.
  uint8_t actionBit; ///< Action bit index in InputMapping, or 255 for special
                     ///< actions.
};

/**
 * @brief Settings scene allowing player to configure keyboard key bindings.
 */
class SettingScene {
public:
  /**
   * @brief Constructs the settings scene.
   * @param dep Client dependencies reference.
   */
  explicit SettingScene(clientDependancy &dep);
  ~SettingScene() = default;

  /// Identifier of this scene.
  static constexpr SceneType Type = SceneType::Setting;

  /**
   * @brief Initializes the settings scene, loads textures and creates UI.
   * @param context Engine services context.
   */
  void onEnter(rtk::EngineContext &context);

  /**
   * @brief Handles user input, navigation and key rebinding.
   * @param context Engine services context.
   * @param inputState Input state of current frame.
   * @param dt Elapsed time since last frame in seconds.
   * @return Next scene type or nullopt to stay in settings.
   */
  std::optional<SceneType> onUpdate(rtk::EngineContext &context,
                                    const rtk::InputState &inputState,
                                    float dt);

  /**
   * @brief Cleans up entities spawned by the settings scene.
   * @param context Engine services context.
   */
  void onExit(rtk::EngineContext &context);

private:
  void createBackground(rtk::EngineContext &context);
  void updateVisuals(rtk::EngineContext &context);
  void clearVisuals(rtk::EngineContext &context);

  clientDependancy &_dependency;
  rtk::Font _font{};
  uint32_t _titleTextureId{0};

  std::vector<std::size_t> _staticEntities{};
  std::vector<std::size_t> _dynamicTextEntities{};

  int _selectedOption{0};
  uint32_t _previousActionMask{0};
  bool _isRebinding{false};
  bool _waitingRelease{false};
  bool _firstFrame{true};
  float _flashTimer{0.0f};

  static const std::vector<BindingOption> OPTIONS;
};

/** @} */
} // namespace rtype::client
