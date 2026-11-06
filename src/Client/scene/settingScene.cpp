#include "settingScene.hpp"

#include <cmath>
#include <string>

#include "components/Transform.hpp"
#include "game.hpp"
#include "input/action.hpp"
#include "systems/RenderConcept.hpp"

namespace rtype::client {

const std::vector<BindingOption> SettingScene::OPTIONS = {
    {"MOVE UP", 0},
    {"MOVE DOWN", 1},
    {"MOVE LEFT", 2},
    {"MOVE RIGHT", 3},
    {"SHOOT / BEAM", 4},
    {"SWITCH WEAPON", 10},
    {"TOGGLE HUD", 7},
    {"READY / UNREADY", 8},
    {"RESTORE DEFAULT CONTROLS", 255},
    {"BACK TO MAIN MENU", 254},
};

namespace {

std::string keyToString(rtk::ecs::concepts::Key key) {
  using Key = rtk::ecs::concepts::Key;
  if (key >= Key::A && key <= Key::Z) {
    return std::string(1, 'A' + (static_cast<int>(key) - static_cast<int>(Key::A)));
  }
  if (key >= Key::Num0 && key <= Key::Num9) {
    return std::string(1, '0' + (static_cast<int>(key) - static_cast<int>(Key::Num0)));
  }
  switch (key) {
  case Key::Up:
    return "UP ARROW";
  case Key::Down:
    return "DOWN ARROW";
  case Key::Left:
    return "LEFT ARROW";
  case Key::Right:
    return "RIGHT ARROW";
  case Key::Space:
    return "SPACE";
  case Key::Enter:
    return "ENTER";
  case Key::Escape:
    return "ESCAPE";
  case Key::Tab:
    return "TAB";
  case Key::Backspace:
    return "BACKSPACE";
  case Key::LShift:
    return "L SHIFT";
  case Key::RShift:
    return "R SHIFT";
  case Key::LControl:
    return "L CTRL";
  case Key::RControl:
    return "R CTRL";
  case Key::LAlt:
    return "L ALT";
  case Key::RAlt:
    return "R ALT";
  case Key::Delete:
    return "DEL";
  case Key::Insert:
    return "INS";
  case Key::Home:
    return "HOME";
  case Key::End:
    return "END";
  case Key::PageUp:
    return "PG UP";
  case Key::PageDown:
    return "PG DOWN";
  case Key::F1:
    return "F1";
  case Key::F2:
    return "F2";
  case Key::F3:
    return "F3";
  case Key::F4:
    return "F4";
  case Key::F5:
    return "F5";
  case Key::F6:
    return "F6";
  case Key::F7:
    return "F7";
  case Key::F8:
    return "F8";
  case Key::F9:
    return "F9";
  case Key::F10:
    return "F10";
  case Key::F11:
    return "F11";
  case Key::F12:
    return "F12";
  default:
    return "KEY " + std::to_string(static_cast<int>(key));
  }
}

void addText(rtk::EngineContext &context, rtk::RenderWindow &window,
             const rtk::Font &font, const std::string &content, float x,
             float y, const rtk::ecs::concepts::ColorRGBA8 &color,
             float scaleFactor, std::vector<std::size_t> &entities) {
  if (!font.isValid() || content.empty())
    return;

  const rtk::Text text = window.createText(
      content, font, {x, y},
      rtk::ColorRGBA8{color.r, color.g, color.b, color.a});

  for (const auto &glyph : text.getSprites()) {
    const auto entity = context.registry.spawn_entity();

    rtk::ecs::concepts::SpriteData sprite{};
    sprite.textureId = glyph.textureId;
    sprite.textureRect.x = glyph.textureRect.left;
    sprite.textureRect.y = glyph.textureRect.top;
    sprite.textureRect.width = glyph.textureRect.width;
    sprite.textureRect.height = glyph.textureRect.height;
    sprite.color = {glyph.color.r, glyph.color.g, glyph.color.b, glyph.color.a};
    sprite.size = {glyph.size.x, glyph.size.y};
    sprite.origin = {glyph.origin.x, glyph.origin.y};
    sprite.scale = {glyph.scale.x * scaleFactor, glyph.scale.y * scaleFactor};
    sprite.layer = 101;
    sprite.flags = 0;

    component::Transform transform{};
    transform.position.x = x + (glyph.position.x - x) * scaleFactor;
    transform.position.y = y + (glyph.position.y - y) * scaleFactor;
    transform.scale = {glyph.scale.x * scaleFactor, glyph.scale.y * scaleFactor};
    transform.rotation = 0.0f;

    context.registry.get_components<component::Transform>().insert_at(
        entity, transform);
    context.registry.get_components<rtk::ecs::concepts::SpriteData>().insert_at(
        entity, sprite);
    entities.push_back(entity);
  }
}

} // namespace

SettingScene::SettingScene(clientDependancy &dep) : _dependency(dep) {}

void SettingScene::createBackground(rtk::EngineContext &context) {
  constexpr float backgroundWidth = 720.0f;

  const auto createLayer = [&](const std::string &blueprint) {
    for (int i = 0; i < 3; ++i) {
      const auto entity = context._entityFactory.createFromBlueprint(
          context.registry, blueprint);

      auto *transform =
          context.registry.get_components<component::Transform>().get(entity);
      if (transform) {
        transform->position.x = static_cast<float>(i) * backgroundWidth;
        transform->position.y = 0.0f;
      }
      _staticEntities.push_back(entity);
    }
  };

  createLayer("background_stars");
  createLayer("background_fast_stars");
}

void SettingScene::onEnter(rtk::EngineContext &context) {
  _font = _dependency.game.getFont();
  if (!_font.isValid()) {
    _font = _dependency.window.loadFont("./assets/font/r-type.otf", 40.0f);
  }

  createBackground(context);

  _titleTextureId =
      _dependency.window.loadTexture("assets/menu_assets/texture/setting.png")
          .getHandle();

  const auto titleEntity = context.registry.spawn_entity();
  component::Transform titleTransform{};
  titleTransform.position = {640.0f - 188.0f, 30.0f};
  titleTransform.scale = {0.15f, 0.15f};
  titleTransform.rotation = 0.0f;

  rtk::ecs::concepts::SpriteData titleSprite{};
  titleSprite.size = {2470.0f, 426.0f};
  titleSprite.origin = {0.0f, 0.0f};
  titleSprite.textureRect = {0, 0, 2470, 426};
  titleSprite.color = {255, 255, 255, 255};
  titleSprite.textureId = _titleTextureId;

  context.registry.get_components<component::Transform>().insert_at(
      titleEntity, titleTransform);
  context.registry.get_components<rtk::ecs::concepts::SpriteData>().insert_at(
      titleEntity, titleSprite);
  _staticEntities.push_back(titleEntity);

  _selectedOption = 0;
  _previousActionMask = 0;
  _isRebinding = false;
  _waitingRelease = false;
  _firstFrame = true;
  _flashTimer = 0.0f;

  updateVisuals(context);
}

void SettingScene::clearVisuals(rtk::EngineContext &context) {
  for (const auto entity : _dynamicTextEntities) {
    context.registry.remove_entity_from_all_pools(entity);
    context.registry.kill_entity(entity);
  }
  context.registry.flush();
  _dynamicTextEntities.clear();
}

void SettingScene::updateVisuals(rtk::EngineContext &context) {
  clearVisuals(context);

  addText(context, _dependency.window, _font, "KEYBOARD CONTROLS CONFIGURATION",
          350.0f, 100.0f, {0, 220, 255, 255}, 0.36f, _dynamicTextEntities);

  constexpr float startY = 150.0f;
  constexpr float rowHeight = 44.0f;
  const int total = static_cast<int>(OPTIONS.size());

  for (int i = 0; i < total; ++i) {
    const float y = startY + static_cast<float>(i) * rowHeight;
    const bool isSelected = (i == _selectedOption);
    const float labelX = isSelected ? 290.0f : 250.0f;
    const float textScale = isSelected ? 0.44f : 0.38f;

    if (OPTIONS[i].actionBit == 255) {
      const rtk::ecs::concepts::ColorRGBA8 color =
          isSelected ? rtk::ecs::concepts::ColorRGBA8{255, 200, 60, 255}
                     : rtk::ecs::concepts::ColorRGBA8{220, 140, 40, 200};
      addText(context, _dependency.window, _font, OPTIONS[i].label,
              labelX, y, color, textScale, _dynamicTextEntities);
      continue;
    }

    if (OPTIONS[i].actionBit == 254) {
      const rtk::ecs::concepts::ColorRGBA8 color =
          isSelected ? rtk::ecs::concepts::ColorRGBA8{255, 255, 255, 255}
                     : rtk::ecs::concepts::ColorRGBA8{180, 180, 180, 200};
      addText(context, _dependency.window, _font, OPTIONS[i].label,
              labelX, y, color, textScale, _dynamicTextEntities);
      continue;
    }

    const rtk::ecs::concepts::ColorRGBA8 labelColor =
        isSelected ? rtk::ecs::concepts::ColorRGBA8{255, 255, 255, 255}
                   : rtk::ecs::concepts::ColorRGBA8{170, 175, 195, 220};

    addText(context, _dependency.window, _font, OPTIONS[i].label,
            labelX, y, labelColor, textScale, _dynamicTextEntities);

    if (_isRebinding && isSelected) {
      addText(context, _dependency.window, _font, "PRESS A KEY",
              730.0f, y, {255, 60, 60, 255}, 0.44f, _dynamicTextEntities);
    } else {
      const auto boundKey = _dependency.game.getInputMapping().getPrimaryBoundKey(
          OPTIONS[i].actionBit);
      const std::string keyLabel = keyToString(boundKey);
      const rtk::ecs::concepts::ColorRGBA8 keyColor =
          isSelected ? rtk::ecs::concepts::ColorRGBA8{255, 230, 80, 255}
                     : rtk::ecs::concepts::ColorRGBA8{80, 220, 255, 240};

      addText(context, _dependency.window, _font, keyLabel, 730.0f, y,
              keyColor, textScale, _dynamicTextEntities);
    }
  }

  addText(context, _dependency.window, _font,
          "UP DOWN TO NAVIGATE   ENTER TO SELECT   ESC TO RETURN",
          240.0f, 630.0f, {130, 130, 160, 200}, 0.34f, _dynamicTextEntities);
}

std::optional<SceneType> SettingScene::onUpdate(rtk::EngineContext &context,
                                               const rtk::InputState &inputState,
                                               float dt) {
  _flashTimer += dt;

  if (_firstFrame) {
    _firstFrame = false;
    _previousActionMask = inputState.actionMask;
    return std::nullopt;
  }

  if (_isRebinding) {
    if (_waitingRelease) {
      if (!_dependency.game.isKeyPressed(rtk::ecs::concepts::Key::Enter) &&
          !_dependency.game.isKeyPressed(rtk::ecs::concepts::Key::Space)) {
        _waitingRelease = false;
      }
      return std::nullopt;
    }

    if (_dependency.game.isKeyPressed(rtk::ecs::concepts::Key::Escape)) {
      _isRebinding = false;
      updateVisuals(context);
      return std::nullopt;
    }

    const auto pressedKey = _dependency.game.getAnyKeyPressed();
    if (pressedKey.has_value() && *pressedKey != rtk::ecs::concepts::Key::Unknown &&
        *pressedKey != rtk::ecs::concepts::Key::Escape) {
      const uint8_t actionBit = OPTIONS[_selectedOption].actionBit;
      _dependency.game.getInputMapping().rebindKey(actionBit, *pressedKey);
      _isRebinding = false;
      updateVisuals(context);
    }
    return std::nullopt;
  }

  const bool upPressed =
      rtype::Input::hasAction(inputState.actionMask, rtype::Action::Up) &&
      !rtype::Input::hasAction(_previousActionMask, rtype::Action::Up);

  const bool downPressed =
      rtype::Input::hasAction(inputState.actionMask, rtype::Action::Down) &&
      !rtype::Input::hasAction(_previousActionMask, rtype::Action::Down);

  const bool confirmPressed =
      rtype::Input::hasAction(inputState.actionMask, rtype::Action::Confirm) &&
      !rtype::Input::hasAction(_previousActionMask, rtype::Action::Confirm);

  const bool escapePressed =
      rtype::Input::hasAction(inputState.actionMask, rtype::Action::LeaveRoom) &&
      !rtype::Input::hasAction(_previousActionMask, rtype::Action::LeaveRoom);

  _previousActionMask = inputState.actionMask;

  if (escapePressed) {
    return SceneType::Menu;
  }

  const int totalOptions = static_cast<int>(OPTIONS.size());
  if (upPressed) {
    _selectedOption = (_selectedOption + totalOptions - 1) % totalOptions;
    updateVisuals(context);
  } else if (downPressed) {
    _selectedOption = (_selectedOption + 1) % totalOptions;
    updateVisuals(context);
  }

  if (confirmPressed) {
    if (_selectedOption == totalOptions - 1) {
      return SceneType::Menu;
    }
    if (_selectedOption == totalOptions - 2) {
      _dependency.game.resetDefaultBindings();
      updateVisuals(context);
      return std::nullopt;
    }

    _isRebinding = true;
    _waitingRelease = true;
    updateVisuals(context);
  }

  return std::nullopt;
}

void SettingScene::onExit(rtk::EngineContext &context) {
  clearVisuals(context);

  for (const auto entity : _staticEntities) {
    context.registry.remove_entity_from_all_pools(entity);
    context.registry.kill_entity(entity);
  }
  context.registry.flush();
  _staticEntities.clear();
}

} // namespace rtype::client
