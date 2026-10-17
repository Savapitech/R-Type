#include <cstdint>

#include "components/Transform.hpp"
#include "systems/RenderConcept.hpp"

#include "introScene.hpp"

#include "../input/action.hpp"

namespace rtype::client {
IntroScene::IntroScene(rtk::RenderWindow &window) : _window(window) {}
void IntroScene::onEnter(rtk::EngineContext &context) {
  utils::vec2 windowSize = std::bit_cast<utils::vec2>(_window.getWindowSize());

  rtk::Texture textureBackground = _window.loadTexture("assets/menu_assets/texture/R-Type_logo.png");
  _textureId = textureBackground.getHandle();

  _logoEntity = context.registry.spawn_entity();
  component::Transform logoTransform{};

  logoTransform.position.x = (windowSize.x / 2) - (textureBackground.getSize().x / 2);
  logoTransform.position.y = (windowSize.y / 2) - (textureBackground.getSize().y / 2);
  logoTransform.rotation = 0.f;
  logoTransform.scale = {1.f, 1.f};

  rtk::ecs::concepts::SpriteData logoSprite{};
  logoSprite.size = {1280.f, 720.f};
  logoSprite.origin = {0.f, 0.f};
  logoSprite.textureRect = {0, 0, 1280, 720};
  logoSprite.color = {255, 255, 255, 255};
  logoSprite.textureId = _textureId;

  context.registry.get_components<component::Transform>().insert_at(_logoEntity, logoTransform);
  context.registry.get_components<rtk::ecs::concepts::SpriteData>().insert_at(_logoEntity, logoSprite);
  _entities.push_back(_logoEntity);

  _fadeEntity = context.registry.spawn_entity();

  component::Transform bgTransform{};
  bgTransform.position = {0.f, 0.f};
  bgTransform.rotation = 0.f;
  bgTransform.scale = {1.f, 1.f};

  rtk::ecs::concepts::SpriteData bgSprite{};
  bgSprite.size = {windowSize.x, windowSize.y};
  bgSprite.origin = {0.f, 0.f};
  bgSprite.textureRect = {0, 0, static_cast<uint16_t>(windowSize.x), static_cast<uint16_t>(windowSize.y)};
  bgSprite.color = {50, 50, 255, 255};
  bgSprite.textureId = _window.loadTexture("assets/entityTexture/Background/withe.jpg").getHandle();

  context.registry.get_components<component::Transform>().insert_at(_fadeEntity, bgTransform);
  context.registry.get_components<rtk::ecs::concepts::SpriteData>().insert_at(_fadeEntity, bgSprite);
  _entities.push_back(_fadeEntity);
}

std::optional<SceneType> IntroScene::onUpdate(rtk::EngineContext &context,
                                             const rtk::InputState &inputState,
                                             float dt) {
  if (!_isTransitioning && rtype::Input::hasAction(inputState.actionMask, rtype::Action::Confirm)){
      _isTransitioning = true;
  }

  if (_currentAlpha > 0.0f) {
      _currentAlpha -= 100.0f * dt;
      if (_currentAlpha < 0.0f) {
          _currentAlpha = 0.0f;
      }
      auto& sprites = context.registry.get_components<rtk::ecs::concepts::SpriteData>();
      if (auto* sprite = sprites.get(_fadeEntity)) {
          sprite->color.a = static_cast<uint8_t>(_currentAlpha);
      }
  }

  if (_isTransitioning) {
      auto& transforms = context.registry.get_components<component::Transform>();

      if (auto* transform = transforms.get(_logoEntity)) {
          constexpr float targetX = 50.0f;
          constexpr float targetY = 50.0f;
          constexpr float targetScale = 0.4f;
          const float animSpeed = 6.0f * dt;

          transform->position.x += (targetX - transform->position.x) * animSpeed;
          transform->position.y += (targetY - transform->position.y) * animSpeed;
          transform->scale.x += (targetScale - transform->scale.x) * animSpeed;
          transform->scale.y += (targetScale - transform->scale.y) * animSpeed;

          if (std::abs(transform->position.x - targetX) < 1.0f) {
              LOG_INFO("Animation over");
              return SceneType::Menu;
          }
      }
  }

  return std::nullopt;
}

void IntroScene::onExit(rtk::EngineContext &context) {
  for (const auto entity : _entities)
    context.registry.kill_entity(entity);

  context.registry.flush();
  _entities.clear();
}
} // namespace rtype::client
