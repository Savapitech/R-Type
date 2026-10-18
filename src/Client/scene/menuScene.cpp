#include "components/Transform.hpp"
#include "systems/RenderConcept.hpp"

#include "menuScene.hpp"

#include "../game.hpp"
#include "../input/action.hpp"

namespace rtype::client {
MenuScene::MenuScene(rtk::RenderWindow &window) : _window(window) {}

void MenuScene::onEnter(rtk::EngineContext &context) {
  _logoTextureId =
      _window.loadTexture("assets/menu_assets/texture/R-Type_logo.png")
          .getHandle();
  _playTextureId =
      _window.loadTexture("assets/menu_assets/texture/play.png").getHandle();
  _settingTextureId =
      _window.loadTexture("assets/menu_assets/texture/setting.png").getHandle();
  _quitTextureId =
      _window.loadTexture("assets/menu_assets/texture/quit.png").getHandle();

  const auto logoEntity = context.registry.spawn_entity();

  component::Transform logoTransform{};
  logoTransform.position = {50.f, 50.f};
  logoTransform.rotation = 0.f;
  logoTransform.scale = {0.4f, 0.4f};

  rtk::ecs::concepts::SpriteData logoSprite{};
  logoSprite.size = {1280.f, 720.f};
  logoSprite.origin = {0.f, 0.f};
  logoSprite.textureRect = {0, 0, 1280, 720};
  logoSprite.color = {255, 255, 255, 255};
  logoSprite.textureId = _logoTextureId;

  context.registry.get_components<component::Transform>().insert_at(
      logoEntity, logoTransform);
  context.registry.get_components<rtk::ecs::concepts::SpriteData>().insert_at(
      logoEntity, logoSprite);
  _entities.push_back(logoEntity);

  struct ButtonInit {
    std::size_t *entityPtr;
    uint32_t textureId;
    float baseWidth;
    float baseHeight;
    uint16_t texWidth;
    uint16_t texHeight;
  };

  ButtonInit buttonInits[3] = {
      {&_playEntity, _playTextureId, 242.f, 65.f, 1600, 430},
      {&_settingEntity, _settingTextureId, 377.f, 65.f, 2470, 426},
      {&_quitEntity, _quitTextureId, 214.f, 65.f, 1413, 430},
  };

  for (auto &init : buttonInits) {
    const auto entity = context.registry.spawn_entity();
    *init.entityPtr = entity;

    component::Transform transform{};
    transform.position = {0.f, 0.f};
    transform.rotation = 0.f;
    transform.scale = {1.f, 1.f};

    rtk::ecs::concepts::SpriteData sprite{};
    sprite.size = {init.baseWidth, init.baseHeight};
    sprite.origin = {0.f, 0.f};
    sprite.textureRect = {0, 0, init.texWidth, init.texHeight};
    sprite.color = {255, 255, 255, 255};
    sprite.textureId = init.textureId;

    context.registry.get_components<component::Transform>().insert_at(
        entity, transform);
    context.registry.get_components<rtk::ecs::concepts::SpriteData>().insert_at(
        entity, sprite);

    _entities.push_back(entity);
  }

  _selectedOption = static_cast<int>(MenuOption::Play);
  _previousActionMask = 0;
  _transitionTime = 0.0f;
  _transitionFinished = false;
  updateTransitionVisuals(context, 0.0f);
}

void MenuScene::updateTransitionVisuals(rtk::EngineContext &context,
                                        float time) {
  auto &transforms = context.registry.get_components<component::Transform>();
  auto &sprites =
      context.registry.get_components<rtk::ecs::concepts::SpriteData>();

  struct ButtonTransitionConfig {
    std::size_t entity;
    float baseWidth;
    float baseHeight;
    float startY;
    float targetCenterY;
    float startTime;
    float endTime;
  };

  ButtonTransitionConfig buttons[3] = {
      {_playEntity, 242.f, 65.f, -80.f, 290.f, 0.0f, 0.50f},
      {_settingEntity, 377.f, 65.f, -80.f, 430.f, 0.15f, 0.70f},
      {_quitEntity, 214.f, 65.f, -80.f, 570.f, 0.30f, 0.90f},
  };

  for (int i = 0; i < 3; ++i) {
    auto *transform = transforms.get(buttons[i].entity);
    auto *sprite = sprites.get(buttons[i].entity);

    if (!transform || !sprite)
      continue;

    float p = 0.0f;
    if (time <= buttons[i].startTime) {
      p = 0.0f;
    } else if (time >= buttons[i].endTime) {
      p = 1.0f;
    } else {
      p = (time - buttons[i].startTime) /
          (buttons[i].endTime - buttons[i].startTime);
    }

    float ease = 0.0f;
    if (p >= 1.0f) {
      ease = 1.0f;
    } else if (p > 0.0f) {
      constexpr float c1 = 0.7f;
      constexpr float c3 = c1 + 1.0f;
      ease =
          1.0f + c3 * std::pow(p - 1.0f, 3.0f) + c1 * std::pow(p - 1.0f, 2.0f);
    }

    const float currentCenterY =
        buttons[i].startY +
        (buttons[i].targetCenterY - buttons[i].startY) * ease;

    const bool isSelected = (i == _selectedOption);
    const float scaleFactor = (isSelected && p >= 1.0f) ? 1.08f : 1.0f;

    transform->scale = {scaleFactor, scaleFactor};
    transform->position.x = 640.f - (buttons[i].baseWidth * scaleFactor) / 2.f;
    transform->position.y =
        currentCenterY - (buttons[i].baseHeight * scaleFactor) / 2.f;

    sprite->size = {buttons[i].baseWidth, buttons[i].baseHeight};

    uint8_t alpha = static_cast<uint8_t>(255.0f * std::min(1.0f, p * 3.0f));
    uint8_t rgb = 255;
    if (p >= 1.0f && !isSelected) {
      rgb = 120;
      alpha = 180;
    } else if (p < 1.0f) {
      rgb = static_cast<uint8_t>(200.0f + 55.0f * p);
    }

    sprite->color = rtk::ecs::concepts::ColorRGBA8{rgb, rgb, rgb, alpha};
  }
}

void MenuScene::updateSelectionVisuals(rtk::EngineContext &context) {
  auto &transforms = context.registry.get_components<component::Transform>();
  auto &sprites =
      context.registry.get_components<rtk::ecs::concepts::SpriteData>();

  struct ButtonLayout {
    std::size_t entity;
    float baseWidth;
    float baseHeight;
    float centerY;
  };

  ButtonLayout buttons[3] = {
      {_playEntity, 242.f, 65.f, 290.f},
      {_settingEntity, 377.f, 65.f, 430.f},
      {_quitEntity, 214.f, 65.f, 570.f},
  };

  for (int i = 0; i < 3; ++i) {
    auto *transform = transforms.get(buttons[i].entity);
    auto *sprite = sprites.get(buttons[i].entity);

    if (!transform || !sprite)
      continue;

    const bool isSelected = (i == _selectedOption);
    const float scaleFactor = isSelected ? 1.08f : 1.0f;

    transform->scale = {scaleFactor, scaleFactor};
    transform->position.x = 640.f - (buttons[i].baseWidth * scaleFactor) / 2.f;
    transform->position.y =
        buttons[i].centerY - (buttons[i].baseHeight * scaleFactor) / 2.f;

    sprite->size = {buttons[i].baseWidth, buttons[i].baseHeight};
    sprite->color = isSelected
                        ? rtk::ecs::concepts::ColorRGBA8{255, 255, 255, 255}
                        : rtk::ecs::concepts::ColorRGBA8{120, 120, 120, 180};
  }
}

std::optional<SceneType> MenuScene::onUpdate(rtk::EngineContext &context,
                                             const rtk::InputState &inputState,
                                             float dt) {
  const bool upPressed =
      rtype::Input::hasAction(inputState.actionMask, rtype::Action::Up) &&
      !rtype::Input::hasAction(_previousActionMask, rtype::Action::Up);

  const bool downPressed =
      rtype::Input::hasAction(inputState.actionMask, rtype::Action::Down) &&
      !rtype::Input::hasAction(_previousActionMask, rtype::Action::Down);

  const bool confirmPressed =
      rtype::Input::hasAction(inputState.actionMask, rtype::Action::Confirm) &&
      !rtype::Input::hasAction(_previousActionMask, rtype::Action::Confirm);

  _previousActionMask = inputState.actionMask;

  if (!_transitionFinished) {
    if (upPressed || downPressed || confirmPressed) {
      _transitionFinished = true;
      _transitionTime = 1.0f;
      updateSelectionVisuals(context);
    } else {
      _transitionTime += dt;
      updateTransitionVisuals(context, _transitionTime);
      if (_transitionTime >= 0.95f) {
        _transitionFinished = true;
        updateSelectionVisuals(context);
      }
      return std::nullopt;
    }
  }

  if (upPressed) {
    _selectedOption = (_selectedOption + 2) % 3;
    updateSelectionVisuals(context);
  } else if (downPressed) {
    _selectedOption = (_selectedOption + 1) % 3;
    updateSelectionVisuals(context);
  }

  if (confirmPressed) {
    switch (static_cast<MenuOption>(_selectedOption)) {
    case MenuOption::Play:
      return SceneType::Connection;
    case MenuOption::Setting:
      return SceneType::Intro;
    case MenuOption::Quit:
      context.eventBus.publish(rtype::events::QuitGameEvent{});
      return std::nullopt;
    default:
      break;
    }
  }

  return std::nullopt;
}

void MenuScene::onExit(rtk::EngineContext &context) {
  for (const auto entity : _entities)
    context.registry.kill_entity(entity);

  context.registry.flush();
  _entities.clear();
}
} // namespace rtype::client