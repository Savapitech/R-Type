#pragma once

#include <ISystem.hpp>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Logger/Logger.hpp"
#include "components/Transform.hpp"
#include "graphical/renderWindow.hpp"
#include "input/action.hpp"
#include "inputAction/InputAction.hpp"
#include "systems/RenderConcept.hpp"

namespace rtk::systems {
/**
 * @addtogroup api_client
 * @{
 */

/**
 * @brief Manages HUD display and bottom overlay elements.
 */
class HudSystem final : public rtk::ecs::ISystem {
public:
  /**
   * @brief Creates the HUD system.
   * @param window Target render window used to load textures and retrieve sizing.
   * @param inputState Input state holding player action masks.
   */
  HudSystem(rtk::RenderWindow &window, const rtk::InputState &inputState)
      : _window(window), _inputState(inputState) {}

  ~HudSystem() override = default;

  /**
   * @brief Initializes the HUD bottom bar entity on start.
   * @param reg Entity registry.
   */
  void onStart(rtk::ecs::Registry &reg) override {
    LOG_INFO("Load HUD System");
    createOrUpdateHudBar(reg);
  }

  /**
   * @brief Handles H key toggle and updates HUD visibility.
   * @param reg Entity registry.
   * @param dt Elapsed time since last frame in seconds.
   */
  void update(rtk::ecs::Registry &reg, float dt) override {
    (void)dt;

    const bool togglePressed =
        rtype::Input::hasAction(_inputState.actionMask, rtype::Action::ToggleHud);
    if (togglePressed && !_wasTogglePressed) {
      _visible = !_visible;
    }
    _wasTogglePressed = togglePressed;

    createOrUpdateHudBar(reg);
  }

  /**
   * @brief Destroys the HUD entity when the system stops.
   * @param reg Entity registry.
   */
  void onStop(rtk::ecs::Registry &reg) override {
    destroyHudBar(reg);
  }

  /**
   * @brief Sets whether the gameplay scene is active.
   * @param inGame True if in game scene.
   */
  void setInGameScene(bool inGame) noexcept {
    _inGameScene = inGame;
  }

  [[nodiscard]] bool isVisible() const noexcept {
    return _visible;
  }

  [[nodiscard]] bool shouldDisplay() const noexcept {
    return _inGameScene && _visible;
  }

  /**
   * @brief Returns the HUD background entity ID if spawned.
   */
  [[nodiscard]] std::optional<std::size_t> getHudEntity() const noexcept {
    return _hudEntity;
  }

private:
  static constexpr float SIDEBAR_WIDTH = 110.0f;
  static constexpr float SIDEBAR_HEIGHT = 720.0f;

  void createOrUpdateHudBar(rtk::ecs::Registry &reg) {
    if (!_hudEntity.has_value() ||
        !reg.has_component<rtk::ecs::concepts::SpriteData>(*_hudEntity)) {
      const auto entity = reg.spawn_entity();
      _hudEntity = entity;

      ::component::Transform transform{};
      transform.position = {0.0f, 0.0f};
      transform.scale = {1.0f, 1.0f};
      transform.rotation = 0.0f;

      rtk::Texture bgTex =
          _window.loadTexture("assets/entityTexture/Background/withe.jpg");

      rtk::ecs::concepts::SpriteData sprite{};
      sprite.textureId = bgTex.getHandle();
      sprite.size = {0.0f, 0.0f};
      sprite.origin = {0.0f, 0.0f};
      sprite.textureRect = {0, 0, static_cast<uint16_t>(SIDEBAR_WIDTH),
                            static_cast<uint16_t>(SIDEBAR_HEIGHT)};
      sprite.color = {0, 0, 0, 0};
      sprite.layer = 100;
      sprite.flags = 0;

      reg.get_components<::component::Transform>().insert_at(entity, transform);
      reg.get_components<rtk::ecs::concepts::SpriteData>().insert_at(entity,
                                                                     sprite);
    }

    auto *sprite =
        reg.get_components<rtk::ecs::concepts::SpriteData>().get(*_hudEntity);
    if (!sprite)
      return;

    if (shouldDisplay()) {
      sprite->size = {SIDEBAR_WIDTH, SIDEBAR_HEIGHT};
      sprite->color = {20, 20, 35, 230};
    } else {
      sprite->size = {0.0f, 0.0f};
      sprite->color = {0, 0, 0, 0};
    }
  }

  void destroyHudBar(rtk::ecs::Registry &reg) {
    if (_hudEntity.has_value()) {
      reg.remove_entity_from_all_pools(*_hudEntity);
      reg.kill_entity(*_hudEntity);
      _hudEntity.reset();
    }
  }

  rtk::RenderWindow &_window;
  const rtk::InputState &_inputState;
  std::optional<std::size_t> _hudEntity{std::nullopt};
  bool _visible = false;
  bool _inGameScene = false;
  bool _wasTogglePressed = false;
};

/** @} */
} // namespace rtk::systems
