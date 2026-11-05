#pragma once

#include <ISystem.hpp>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#include "Logger/Logger.hpp"
#include "components/Controllable.hpp"
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
 * @brief Manages HUD display and left sidebar overlay elements.
 */
class HudSystem final : public rtk::ecs::ISystem {
public:
  /**
   * @brief Creates the HUD system.
   * @param window Target render window used to load textures and fonts.
   * @param inputState Input state holding player action masks.
   * @param font Font used to render text strings.
   */
  HudSystem(rtk::RenderWindow &window, const rtk::InputState &inputState,
            rtk::Font font = {})
      : _window(window), _inputState(inputState), _font(font) {}

  ~HudSystem() override = default;

  /**
   * @brief Initializes the HUD sidebar on start.
   * @param reg Entity registry.
   */
  void onStart(rtk::ecs::Registry &reg) override {
    LOG_INFO("Load HUD System");
    if (!_font.isValid()) {
      _font = _window.loadFont("./assets/font/r-type.otf", 40.0f);
    }
    createOrUpdateHudBar(reg);
  }

  /**
   * @brief Handles H key toggle, time progression, and text display updates.
   * @param reg Entity registry.
   * @param dt Elapsed time since last frame in seconds.
   */
  void update(rtk::ecs::Registry &reg, float dt) override {
    const bool togglePressed = rtype::Input::hasAction(
        _inputState.actionMask, rtype::Action::ToggleHud);
    if (togglePressed && !_wasTogglePressed) {
      _visible = !_visible;
    }
    _wasTogglePressed = togglePressed;

    createOrUpdateHudBar(reg);

    _elapsedTime += dt;
    _textUpdateTimer += dt;

    const bool displayStateChanged = (shouldDisplay() != _lastDisplayed);
    _lastDisplayed = shouldDisplay();

    if (displayStateChanged) {
      updateTextEntities(reg);
      _textUpdateTimer = 0.0f;
    } else if (shouldDisplay() && _textUpdateTimer >= 0.2f) {
      updateTextEntities(reg);
      _textUpdateTimer = 0.0f;
    }
  }

  /**
   * @brief Destroys the HUD entities when the system stops.
   * @param reg Entity registry.
   */
  void onStop(rtk::ecs::Registry &reg) override {
    clearTextEntities(reg);
    destroyHudBar(reg);
  }

  /**
   * @brief Sets whether the gameplay scene is active.
   * @param inGame True if in game scene.
   */
  void setInGameScene(bool inGame) noexcept { _inGameScene = inGame; }

  /**
   * @brief Updates the active font handle.
   * @param font Valid font handle.
   */
  void setFont(rtk::Font font) noexcept { _font = font; }

  /**
   * @brief Increments the counter of bullets fired by the player.
   */
  void incrementShotCount() noexcept { ++_shotsFired; }

  /**
   * @brief Sets the counter of bullets fired.
   * @param shots Total shot count.
   */
  void setShotCount(std::size_t shots) noexcept { _shotsFired = shots; }

  /**
   * @brief Returns the total number of bullets fired.
   * @return Count of shots fired.
   */
  [[nodiscard]] std::size_t getShotCount() const noexcept {
    return _shotsFired;
  }

  /**
   * @brief Increments the collected speed bonus count.
   */
  void addSpeedBonus() noexcept { ++_speedBonusCount; }

  /**
   * @brief Sets the collected speed bonus count.
   * @param count Speed bonus count.
   */
  void setSpeedBonusCount(int count) noexcept { _speedBonusCount = count; }

  /**
   * @brief Returns the collected speed bonus count.
   * @return Active speed bonuses count.
   */
  [[nodiscard]] int getSpeedBonusCount() const noexcept {
    return _speedBonusCount;
  }

  [[nodiscard]] bool isVisible() const noexcept { return _visible; }

  [[nodiscard]] bool shouldDisplay() const noexcept {
    return _inGameScene && _visible;
  }

  [[nodiscard]] std::optional<std::size_t> getHudEntity() const noexcept {
    return _hudEntity;
  }

private:
  static constexpr float SIDEBAR_WIDTH = 190.0f;
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

    if (!_borderEntity.has_value() ||
        !reg.has_component<rtk::ecs::concepts::SpriteData>(*_borderEntity)) {
      const auto border = reg.spawn_entity();
      _borderEntity = border;

      ::component::Transform borderTransform{};
      borderTransform.position = {SIDEBAR_WIDTH - 2.0f, 0.0f};
      borderTransform.scale = {1.0f, 1.0f};
      borderTransform.rotation = 0.0f;

      rtk::Texture bgTex =
          _window.loadTexture("assets/entityTexture/Background/withe.jpg");

      rtk::ecs::concepts::SpriteData borderSprite{};
      borderSprite.textureId = bgTex.getHandle();
      borderSprite.size = {0.0f, 0.0f};
      borderSprite.origin = {0.0f, 0.0f};
      borderSprite.textureRect = {0, 0, 2,
                                  static_cast<uint16_t>(SIDEBAR_HEIGHT)};
      borderSprite.color = {0, 0, 0, 0};
      borderSprite.layer = 100;
      borderSprite.flags = 0;

      reg.get_components<::component::Transform>().insert_at(border,
                                                             borderTransform);
      reg.get_components<rtk::ecs::concepts::SpriteData>().insert_at(
          border, borderSprite);
    }

    auto *sprite =
        reg.get_components<rtk::ecs::concepts::SpriteData>().get(*_hudEntity);
    auto *borderSprite =
        reg.get_components<rtk::ecs::concepts::SpriteData>().get(
            *_borderEntity);

    if (shouldDisplay()) {
      if (sprite) {
        sprite->size = {SIDEBAR_WIDTH, SIDEBAR_HEIGHT};
        sprite->color = {15, 18, 30, 240};
      }
      if (borderSprite) {
        borderSprite->size = {2.0f, SIDEBAR_HEIGHT};
        borderSprite->color = {0, 190, 255, 220};
      }
    } else {
      if (sprite) {
        sprite->size = {0.0f, 0.0f};
        sprite->color = {0, 0, 0, 0};
      }
      if (borderSprite) {
        borderSprite->size = {0.0f, 0.0f};
        borderSprite->color = {0, 0, 0, 0};
      }
    }
  }

  void addText(rtk::ecs::Registry &reg, const std::string &content, float x,
               float y, const rtk::ecs::concepts::ColorRGBA8 &color,
               float scaleFactor = 0.45f) {
    if (!_font.isValid()) {
      _font = _window.loadFont("./assets/font/r-type.otf", 40.0f);
    }
    if (!_font.isValid() || content.empty())
      return;

    const rtk::Text text =
        _window.createText(content, _font, {x, y},
                           rtk::ColorRGBA8{color.r, color.g, color.b, color.a});

    for (const auto &glyph : text.getSprites()) {
      const auto entity = reg.spawn_entity();

      rtk::ecs::concepts::SpriteData sprite{};
      sprite.textureId = glyph.textureId;
      sprite.textureRect.x = glyph.textureRect.left;
      sprite.textureRect.y = glyph.textureRect.top;
      sprite.textureRect.width = glyph.textureRect.width;
      sprite.textureRect.height = glyph.textureRect.height;
      sprite.color = {glyph.color.r, glyph.color.g, glyph.color.b,
                      glyph.color.a};
      sprite.size = {glyph.size.x, glyph.size.y};
      sprite.origin = {glyph.origin.x, glyph.origin.y};
      sprite.scale = {glyph.scale.x * scaleFactor, glyph.scale.y * scaleFactor};
      sprite.layer = 101;
      sprite.flags = 0;

      ::component::Transform transform{};
      transform.position.x = x + (glyph.position.x - x) * scaleFactor;
      transform.position.y = y + (glyph.position.y - y) * scaleFactor;
      transform.scale = {glyph.scale.x * scaleFactor,
                         glyph.scale.y * scaleFactor};
      transform.rotation = 0.0f;

      reg.get_components<::component::Transform>().insert_at(entity, transform);
      reg.get_components<rtk::ecs::concepts::SpriteData>().insert_at(entity,
                                                                     sprite);

      _textEntities.push_back(entity);
    }
  }

  void addSprite(rtk::ecs::Registry &reg, uint32_t textureId, float x, float y,
                 float width, float height,
                 const rtk::ecs::concepts::ColorRGBA8 &color,
                 float scale = 1.0f) {
    const auto entity = reg.spawn_entity();

    component::Transform transform{};
    transform.position = {x, y};
    transform.scale = {scale, scale};
    transform.rotation = 0.0f;

    rtk::ecs::concepts::SpriteData sprite{};
    sprite.textureId = textureId;
    sprite.size = {width * scale, height * scale};
    sprite.origin = {0.0f, 0.0f};
    sprite.textureRect = {0, 0, static_cast<uint16_t>(width),
                          static_cast<uint16_t>(height)};
    sprite.color = color;
    sprite.scale = {scale, scale};
    sprite.layer = 101;
    sprite.flags = 0;

    reg.get_components<component::Transform>().insert_at(entity, transform);
    reg.get_components<rtk::ecs::concepts::SpriteData>().insert_at(entity,
                                                                   sprite);
    _textEntities.push_back(entity);
  }

  void updateTextEntities(rtk::ecs::Registry &reg) {
    clearTextEntities(reg);

    if (!shouldDisplay())
      return;

    addText(reg, "R_TYPE HUD", 18.0f, 30.0f, {0, 220, 255, 255}, 0.42f);

    const int totalSec = static_cast<int>(_elapsedTime);
    const int minutes = totalSec / 60;
    const int seconds = totalSec % 60;
    char timeBuf[16];
    std::snprintf(timeBuf, sizeof(timeBuf), "%02d.%02d", minutes, seconds);

    addText(reg, "TIME", 18.0f, 75.0f, {170, 170, 195, 255}, 0.36f);
    addText(reg, timeBuf, 18.0f, 98.0f, {255, 230, 80, 255}, 0.52f);

    std::size_t count = 0;
    bool isCharging = false;
    for (auto ent : reg.view<component::Controllable>()) {
      count++;
      if (const auto *ctrl =
              reg.get_components<component::Controllable>().get(ent)) {
        if (ctrl->isChargingShoot) {
          isCharging = true;
        }
      }
    }
    const std::string playersStr =
        std::to_string(std::max<std::size_t>(1, count)) + " ONLINE";

    addText(reg, "PLAYERS", 18.0f, 155.0f, {170, 170, 195, 255}, 0.36f);
    addText(reg, playersStr, 18.0f, 178.0f, {80, 255, 120, 255}, 0.44f);

    addText(reg, "SYSTEM", 18.0f, 235.0f, {170, 170, 195, 255}, 0.36f);
    addText(reg, "ACTIVE", 18.0f, 258.0f, {50, 220, 255, 255}, 0.46f);

    addText(reg, "STAGE", 18.0f, 315.0f, {170, 170, 195, 255}, 0.36f);
    addText(reg, "01", 18.0f, 338.0f, {255, 255, 255, 255}, 0.52f);

    addText(reg, "BEAM", 18.0f, 395.0f, {170, 170, 195, 255}, 0.36f);
    if (isCharging) {
      addText(reg, "CHARGING", 18.0f, 418.0f, {255, 220, 0, 255}, 0.44f);
    } else {
      addText(reg, "READY", 18.0f, 418.0f, {255, 160, 40, 255}, 0.46f);
    }

    addText(reg, "SHOTS", 18.0f, 465.0f, {170, 170, 195, 255}, 0.36f);
    addText(reg, std::to_string(_shotsFired), 18.0f, 488.0f,
            {255, 120, 120, 255}, 0.50f);

    addText(reg, "BONUS", 18.0f, 535.0f, {170, 170, 195, 255}, 0.36f);

    int speedBonus = _speedBonusCount;
    for (auto ent : reg.view<component::Controllable>()) {
      if (const auto *ctrl =
              reg.get_components<component::Controllable>().get(ent)) {
        if (ctrl->speed > 300.0f) {
          const int countFromSpeed =
              static_cast<int>((ctrl->speed - 300.0f) / 100.0f + 0.5f);
          speedBonus = std::max(speedBonus, countFromSpeed);
        }
      }
    }

    if (!_speedTexture.isValid()) {
      _speedTexture = _window.loadTexture("assets/entityTexture/speedBoost.png");
    }

    if (_speedTexture.isValid()) {
      if (speedBonus > 0) {
        addSprite(reg, _speedTexture.getHandle(), 18.0f, 560.0f, 32.0f, 32.0f,
                  {255, 255, 255, 255}, 0.85f);
        addText(reg, "SPEED", 50.0f, 558.0f, {0, 240, 255, 255}, 0.34f);
        addText(reg, "LVL " + std::to_string(speedBonus), 50.0f, 574.0f,
                {255, 220, 0, 255}, 0.34f);
      } else {
        addSprite(reg, _speedTexture.getHandle(), 18.0f, 560.0f, 32.0f, 32.0f,
                  {70, 70, 90, 100}, 0.85f);
        addText(reg, "SPEED", 50.0f, 558.0f, {100, 100, 120, 130}, 0.34f);
        addText(reg, "NONE", 50.0f, 574.0f, {100, 100, 120, 130}, 0.32f);
      }
    }

    addText(reg, "H  TOGGLE", 18.0f, 680.0f, {120, 120, 150, 200}, 0.34f);
  }

  void clearTextEntities(rtk::ecs::Registry &reg) {
    for (auto ent : _textEntities) {
      reg.remove_entity_from_all_pools(ent);
      reg.kill_entity(ent);
    }
    _textEntities.clear();
  }

  void destroyHudBar(rtk::ecs::Registry &reg) {
    if (_hudEntity.has_value()) {
      reg.remove_entity_from_all_pools(*_hudEntity);
      reg.kill_entity(*_hudEntity);
      _hudEntity.reset();
    }
    if (_borderEntity.has_value()) {
      reg.remove_entity_from_all_pools(*_borderEntity);
      reg.kill_entity(*_borderEntity);
      _borderEntity.reset();
    }
  }

  rtk::RenderWindow &_window;
  const rtk::InputState &_inputState;
  rtk::Font _font{};
  std::optional<std::size_t> _hudEntity{std::nullopt};
  std::optional<std::size_t> _borderEntity{std::nullopt};
  std::vector<std::size_t> _textEntities{};

  bool _visible = false;
  bool _inGameScene = false;
  bool _wasTogglePressed = false;
  bool _lastDisplayed = false;

  float _elapsedTime = 0.0f;
  float _textUpdateTimer = 0.0f;
  std::size_t _shotsFired = 0;
  rtk::Texture _speedTexture{};
  int _speedBonusCount = 0;
};

/** @} */
} // namespace rtk::systems
