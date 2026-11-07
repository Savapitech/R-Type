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
#include "events/GameEvents.hpp"
#include "events/busEvents.hpp"
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
   * @brief Creates the HUD system with event bus subscription.
   * @param window Target render window used to load textures and fonts.
   * @param inputState Input state holding player action masks.
   * @param eventBus Shared bus used to listen to beam charge and gameplay events.
   * @param font Font used to render text strings.
   */
  HudSystem(rtk::RenderWindow &window, const rtk::InputState &inputState,
            busEvent &eventBus, rtk::Font font = {})
      : _window(window), _inputState(inputState), _eventBus(&eventBus),
        _font(font) {
    _eventBus->subscribe<rtype::events::BeamChargeEvent>(
        [this](const rtype::events::BeamChargeEvent &event) {
          _beamChargeRatio = event.chargeRatio;
          _beamChargeLevel = event.chargeLevel;
          _isBeamCharging = event.isCharging;
        });
  }

  /**
   * @brief Creates the HUD system.
   * @param window Target render window used to load textures and fonts.
   * @param inputState Input state holding player action masks.
   * @param font Font used to render text strings.
   */
  HudSystem(rtk::RenderWindow &window, const rtk::InputState &inputState,
            rtk::Font font = {})
      : _window(window), _inputState(inputState), _eventBus(nullptr),
        _font(font) {}

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
    ensureWhiteTexture();
    if (!_speedTexture.isValid()) {
      _speedTexture = _window.loadTexture("assets/entityTexture/speedBoost.png");
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

    if (_weaponTimer > 0.0f) {
      _weaponTimer -= dt;
      if (_weaponTimer <= 0.0f) {
        _weaponTimer = 0.0f;
        _currentWeapon = 0;
      }
    }

    createOrUpdateHudBar(reg);

    _elapsedTime += dt;
    _textUpdateTimer += dt;

    const bool displayStateChanged = (shouldDisplay() != _lastDisplayed);
    _lastDisplayed = shouldDisplay();

    const float updateInterval =
        (_isBeamCharging || (_currentWeapon != 0 && _weaponTimer > 0.0f))
            ? 0.033f
            : 0.2f;

    if (displayStateChanged) {
      updateTextEntities(reg);
      _textUpdateTimer = 0.0f;
    } else if (shouldDisplay() && _textUpdateTimer >= updateInterval) {
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
   * @brief Updates the beam charge gauge status.
   * @param ratio Fill ratio from 0.0 to 1.0.
   * @param level Charge stage (0: charging, 1: stage 1, 2: stage 2, 3: max).
   * @param isCharging True while the beam button is held.
   */
  void setBeamCharge(float ratio, int level, bool isCharging) noexcept {
    _beamChargeRatio = ratio;
    _beamChargeLevel = level;
    _isBeamCharging = isCharging;
  }

  [[nodiscard]] float getBeamChargeRatio() const noexcept {
    return _beamChargeRatio;
  }

  [[nodiscard]] int getBeamChargeLevel() const noexcept {
    return _beamChargeLevel;
  }

  [[nodiscard]] bool isBeamCharging() const noexcept {
    return _isBeamCharging;
  }

  static constexpr int MAX_SPEED_LEVEL = 4;
  static constexpr float BASE_SPEED = 300.0f;
  static constexpr float SPEED_STEP = 35.0f;
  static constexpr float MAX_SPEED = BASE_SPEED + MAX_SPEED_LEVEL * SPEED_STEP;

  /**
   * @brief Increments the collected speed bonus count.
   */
  void addSpeedBonus() noexcept {
    if (_speedBonusCount < MAX_SPEED_LEVEL) {
      ++_speedBonusCount;
    }
  }

  /**
   * @brief Sets the collected speed bonus count.
   * @param count Speed bonus count.
   */
  void setSpeedBonusCount(int count) noexcept {
    _speedBonusCount = std::clamp(count, 0, MAX_SPEED_LEVEL);
  }

  /**
   * @brief Resets the collected speed bonus count.
   */
  void resetSpeedBonus() noexcept { _speedBonusCount = 0; }

  /**
   * @brief Returns the collected speed bonus count.
   * @return Active speed bonuses count.
   */
  [[nodiscard]] int getSpeedBonusCount() const noexcept {
    return _speedBonusCount;
  }

  /**
   * @brief Sets the active weapon and starts the power-up duration timer.
   * @param weapon 0: Cannon, 1: Spread, 2: Plasma.
   * @param duration Power-up duration in seconds.
   */
  void setWeaponWithTimer(std::uint8_t weapon, float duration) noexcept {
    _currentWeapon = static_cast<std::uint8_t>(weapon % 3);
    _weaponTimer = duration;
    _weaponMaxDuration = (duration > 0.0f) ? duration : 5.0f;
    _textUpdateTimer = 999.0f;
  }

  /**
   * @brief Sets the active weapon index.
   * @param weapon 0: Cannon, 1: Spread, 2: Plasma.
   */
  void setWeapon(std::uint8_t weapon) noexcept {
    _currentWeapon = static_cast<std::uint8_t>(weapon % 3);
    _textUpdateTimer = 999.0f;
  }

  /**
   * @brief Returns the active weapon index.
   * @return Active weapon index.
   */
  [[nodiscard]] std::uint8_t getWeapon() const noexcept {
    return _currentWeapon;
  }

  /**
   * @brief Returns the remaining weapon power-up timer in seconds.
   * @return Weapon timer in seconds.
   */
  [[nodiscard]] float getWeaponTimer() const noexcept {
    return _weaponTimer;
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

  void ensureWhiteTexture() {
    if (!_whiteTexture.isValid()) {
      static const std::uint8_t whitePixel[4] = {255, 255, 255, 255};
      _whiteTexture = _window.loadTextureFromMemory(
          std::span<const std::uint8_t>(whitePixel, 4), 1, 1);
      if (!_whiteTexture.isValid()) {
        _whiteTexture =
            _window.loadTexture("assets/entityTexture/Background/withe.jpg");
      }
    }
  }

  void addSolidRect(rtk::ecs::Registry &reg, float x, float y, float width,
                    float height, const rtk::ecs::concepts::ColorRGBA8 &color) {
    if (width <= 0.0f || height <= 0.0f)
      return;
    ensureWhiteTexture();
    if (!_whiteTexture.isValid())
      return;

    const auto entity = reg.spawn_entity();

    component::Transform transform{};
    transform.position = {x, y};
    transform.scale = {1.0f, 1.0f};
    transform.rotation = 0.0f;

    rtk::ecs::concepts::SpriteData sprite{};
    sprite.textureId = _whiteTexture.getHandle();
    sprite.size = {width, height};
    sprite.origin = {0.0f, 0.0f};
    sprite.textureRect = {0, 0, 1, 1};
    sprite.color = color;
    sprite.scale = {1.0f, 1.0f};
    sprite.layer = 101;
    sprite.flags = 0;

    reg.get_components<component::Transform>().insert_at(entity, transform);
    reg.get_components<rtk::ecs::concepts::SpriteData>().insert_at(entity,
                                                                   sprite);
    _textEntities.push_back(entity);
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
        if (ctrl->weaponTimer > 0.0f &&
            (ctrl->weaponType != _currentWeapon ||
             ctrl->weaponTimer > _weaponTimer)) {
          _currentWeapon = ctrl->weaponType;
          _weaponTimer = ctrl->weaponTimer;
          _weaponMaxDuration = ctrl->weaponMaxDuration;
        }
      }
    }
    const std::string playersStr =
        std::to_string(std::max<std::size_t>(1, count)) + " ONLINE";

    addText(reg, "PLAYERS", 18.0f, 155.0f, {170, 170, 195, 255}, 0.36f);
    addText(reg, playersStr, 18.0f, 178.0f, {80, 255, 120, 255}, 0.44f);

    addText(reg, "WEAPON", 18.0f, 235.0f, {170, 170, 195, 255}, 0.36f);

    if (_currentWeapon == 1 && _weaponTimer > 0.0f) {
      addText(reg, "SPREAD", 18.0f, 258.0f, {255, 200, 40, 255}, 0.46f);
    } else if (_currentWeapon == 2 && _weaponTimer > 0.0f) {
      addText(reg, "PLASMA", 18.0f, 258.0f, {255, 45, 55, 255}, 0.46f);
    } else {
      addText(reg, "CANNON", 18.0f, 258.0f, {50, 220, 255, 255}, 0.46f);
      addText(reg, "STANDARD", 18.0f, 280.0f, {120, 120, 150, 180}, 0.30f);
    }

    if (_currentWeapon != 0 && _weaponTimer > 0.0f) {
      const float maxDur =
          (_weaponMaxDuration > 0.0f) ? _weaponMaxDuration : 5.0f;
      const float ratio = std::clamp(_weaponTimer / maxDur, 0.0f, 1.0f);
      constexpr float trackX = 18.0f;
      constexpr float trackY = 282.0f;
      constexpr float trackWidth = 154.0f;
      constexpr float trackHeight = 8.0f;

      addSolidRect(reg, trackX, trackY, trackWidth, trackHeight,
                   {25, 30, 45, 240});

      const float fillWidth = trackWidth * ratio;
      if (fillWidth > 0.5f) {
        const rtk::ecs::concepts::ColorRGBA8 barColor =
            (_currentWeapon == 1)
                ? rtk::ecs::concepts::ColorRGBA8{255, 200, 40, 255}
                : rtk::ecs::concepts::ColorRGBA8{255, 45, 55, 255};
        addSolidRect(reg, trackX, trackY, fillWidth, trackHeight, barColor);
      }

      char secBuf[16];
      std::snprintf(secBuf, sizeof(secBuf), "%.1fs", _weaponTimer);
      addText(reg, secBuf, 18.0f, 296.0f, {230, 235, 250, 240}, 0.32f);
    }

    addText(reg, "STAGE", 18.0f, 326.0f, {170, 170, 195, 255}, 0.36f);
    addText(reg, "01", 18.0f, 349.0f, {255, 255, 255, 255}, 0.52f);

    addText(reg, "BEAM", 18.0f, 400.0f, {170, 170, 195, 255}, 0.36f);
    if (_isBeamCharging || isCharging) {
      if (_beamChargeLevel == 3) {
        addText(reg, "MAX BEAM!", 18.0f, 423.0f, {255, 60, 60, 255}, 0.46f);
      } else if (_beamChargeLevel == 2) {
        addText(reg, "STAGE 2", 18.0f, 423.0f, {200, 120, 255, 255}, 0.44f);
      } else if (_beamChargeLevel == 1) {
        addText(reg, "STAGE 1", 18.0f, 423.0f, {80, 220, 255, 255}, 0.44f);
      } else {
        addText(reg, "CHARGING", 18.0f, 423.0f, {255, 220, 0, 255}, 0.44f);
      }

      constexpr float trackX = 18.0f;
      constexpr float trackY = 447.0f;
      constexpr float trackWidth = 154.0f;
      constexpr float trackHeight = 8.0f;

      addSolidRect(reg, trackX, trackY, trackWidth, trackHeight,
                   {25, 30, 45, 240});

      const float fillWidth =
          trackWidth * std::clamp(_beamChargeRatio, 0.0f, 1.0f);
      if (fillWidth > 0.5f) {
        rtk::ecs::concepts::ColorRGBA8 beamColor = {255, 220, 0, 255};
        if (_beamChargeLevel == 1) {
          beamColor = {80, 220, 255, 255};
        } else if (_beamChargeLevel == 2) {
          beamColor = {200, 120, 255, 255};
        } else if (_beamChargeLevel == 3) {
          beamColor = {255, 60, 60, 255};
        }
        addSolidRect(reg, trackX, trackY, fillWidth, trackHeight, beamColor);
      }
    } else {
      addText(reg, "READY", 18.0f, 423.0f, {255, 160, 40, 255}, 0.46f);

      constexpr float trackX = 18.0f;
      constexpr float trackY = 447.0f;
      constexpr float trackWidth = 154.0f;
      constexpr float trackHeight = 8.0f;

      addSolidRect(reg, trackX, trackY, trackWidth, trackHeight,
                   {25, 30, 45, 140});
    }

    addText(reg, "SHOTS", 18.0f, 468.0f, {170, 170, 195, 255}, 0.36f);
    addText(reg, std::to_string(_shotsFired), 18.0f, 491.0f,
            {255, 120, 120, 255}, 0.50f);

    addText(reg, "BONUS", 18.0f, 535.0f, {170, 170, 195, 255}, 0.36f);

    int speedBonus = _speedBonusCount;
    for (auto ent : reg.view<component::Controllable>()) {
      if (const auto *ctrl =
              reg.get_components<component::Controllable>().get(ent)) {
        if (ctrl->speed > BASE_SPEED) {
          const int countFromSpeed =
              static_cast<int>((ctrl->speed - BASE_SPEED) / SPEED_STEP + 0.5f);
          speedBonus = std::max(speedBonus, countFromSpeed);
        }
      }
    }
    speedBonus = std::clamp(speedBonus, 0, MAX_SPEED_LEVEL);

    if (!_speedTexture.isValid()) {
      _speedTexture = _window.loadTexture("assets/entityTexture/speedBoost.png");
    }

    if (_speedTexture.isValid()) {
      if (speedBonus > 0) {
        addSprite(reg, _speedTexture.getHandle(), 18.0f, 558.0f, 32.0f, 32.0f,
                  {0, 240, 255, 255}, 0.85f);
        addText(reg, "SPEED", 50.0f, 556.0f, {0, 240, 255, 255}, 0.34f);
        if (speedBonus >= MAX_SPEED_LEVEL) {
          addText(reg, "LVL " + std::to_string(speedBonus) + " (MAX)", 50.0f,
                  572.0f, {255, 180, 0, 255}, 0.30f);
        } else {
          addText(reg, "LVL " + std::to_string(speedBonus), 50.0f, 572.0f,
                  {255, 220, 0, 255}, 0.34f);
        }
      } else {
        addSprite(reg, _speedTexture.getHandle(), 18.0f, 558.0f, 32.0f, 32.0f,
                  {70, 70, 90, 100}, 0.85f);
        addText(reg, "SPEED", 50.0f, 556.0f, {100, 100, 120, 130}, 0.34f);
        addText(reg, "NONE", 50.0f, 572.0f, {100, 100, 120, 130}, 0.32f);
      }

      if (_currentWeapon == 2 && _weaponTimer > 0.0f) {
        addSprite(reg, _speedTexture.getHandle(), 18.0f, 604.0f, 32.0f, 32.0f,
                  {255, 45, 55, 255}, 0.85f);
        addText(reg, "PLASMA", 50.0f, 602.0f, {255, 45, 55, 255}, 0.34f);
        char bTimerBuf[16];
        std::snprintf(bTimerBuf, sizeof(bTimerBuf), "%.1fs", _weaponTimer);
        addText(reg, bTimerBuf, 50.0f, 618.0f, {255, 200, 200, 255}, 0.32f);
      } else if (_currentWeapon == 1 && _weaponTimer > 0.0f) {
        addSprite(reg, _speedTexture.getHandle(), 18.0f, 604.0f, 32.0f, 32.0f,
                  {255, 200, 40, 255}, 0.85f);
        addText(reg, "SPREAD", 50.0f, 602.0f, {255, 200, 40, 255}, 0.34f);
        char bTimerBuf[16];
        std::snprintf(bTimerBuf, sizeof(bTimerBuf), "%.1fs", _weaponTimer);
        addText(reg, bTimerBuf, 50.0f, 618.0f, {255, 240, 180, 255}, 0.32f);
      } else {
        addSprite(reg, _speedTexture.getHandle(), 18.0f, 604.0f, 32.0f, 32.0f,
                  {70, 70, 90, 100}, 0.85f);
        addText(reg, "WEAPON", 50.0f, 602.0f, {100, 100, 120, 130}, 0.34f);
        addText(reg, "STANDARD", 50.0f, 618.0f, {100, 100, 120, 130}, 0.30f);
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
  bool _wasSwitchPressed = false;
  bool _lastDisplayed = false;

  float _elapsedTime = 0.0f;
  float _textUpdateTimer = 0.0f;
  std::size_t _shotsFired = 0;
  rtk::Texture _speedTexture{};
  int _speedBonusCount = 0;
  std::uint8_t _currentWeapon = 0;
  float _weaponTimer = 0.0f;
  float _weaponMaxDuration = 5.0f;
  busEvent *_eventBus = nullptr;
  float _beamChargeRatio = 0.0f;
  int _beamChargeLevel = 0;
  bool _isBeamCharging = false;
  rtk::Texture _whiteTexture{};
};

/** @} */
} // namespace rtk::systems
