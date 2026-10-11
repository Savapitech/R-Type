#include "gameScene.hpp"
#include "components/AABBCollider.hpp"
#include "components/Transform.hpp"
#include "components/Controllable.hpp"
#include "components/Velocity.hpp"

#include "systems/AnimationSystem.hpp"
#include "systems/RenderConcept.hpp"


#include "../input/action.hpp"

namespace rtype::client {
GameScene::GameScene(rtk::RenderWindow &window) : _window(window) {}

void GameScene::onEnter(rtk::EngineContext &context) {
  bullets.reserve(200);

  for (int i = 0; i < 200; i++)
    bullets.push_back(0);

  _textureId = _window.loadTexture("sprite.png").getHandle();

  for (int i = 0; i < 3; ++i) {
    const auto entity = context.registry.spawn_entity();

    if (i == 0)
      _playerEntity = entity;

    component::Transform transform{};
    transform.position = {150.f + static_cast<float>(i) * 250.f, 250.f};
    transform.rotation = 0.f;
    transform.scale = {1.f, 1.f};

    rtk::ecs::concepts::SpriteData sprite{};
    sprite.size = {64.f, 32.f};
    sprite.origin = {0.f, 0.f};
    sprite.textureRect = {0, 0, 32, 16};
    sprite.color = {255, 255, 255, 255};
    sprite.textureId = _textureId;

    component::AABBCollider collider{};
    collider.offsetX = 0.f;
    collider.offsetY = 0.f;
    collider.width = 64.f;
    collider.height = 32.f;
    collider.layer = (i == 0 || i == 1) ? Collider::CollisionLayer::Player
                              : Collider::CollisionLayer::Enemy;
    collider.mask = (collider.layer & Collider::CollisionLayer::Player)
                        ? Collider::CollisionLayer::Enemy |
                              Collider::CollisionLayer::EnemyProjectile
                        : Collider::CollisionLayer::Player |
                              Collider::CollisionLayer::PlayerProjectile |
                              Collider::CollisionLayer::Enemy;

    component::Animation animation{};
    animation.frameDuration = 0.15f;
    animation.currentFrame = 0;

    animation.totalFrames = 4 * 5;
    animation.frameWidth = 32;
    animation.frameHeight = 17;
    animation.startX = 0;
    animation.startY = 0;
    animation.loop = true;
    animation.playing = true;

    animation.columns = 5;
    animation.paddingX = 0;
    animation.paddingY = 0;

    context.registry.get_components<component::Transform>().insert_at(
        entity, transform);
    context.registry.get_components<rtk::ecs::concepts::SpriteData>().insert_at(
        entity, sprite);
    context.registry.get_components<component::AABBCollider>().insert_at(
        entity, collider);

    context.registry.get_components<component::Animation>().insert_at(
        entity, animation);

    context.registry.get_components<component::Controllable>().insert_at(
      entity, {});
    context.registry.get_components<component::Velocity>().insert_at(
      entity, {});

    _entities.push_back(entity);
  }
}

std::optional<SceneType> GameScene::onUpdate(rtk::EngineContext &context,
                                             const rtk::InputState &inputState,
                                             float dt) {

  return std::nullopt;
}

void GameScene::onExit(rtk::EngineContext &context) {
  for (const auto entity : _entities)
    context.registry.kill_entity(entity);

  context.registry.flush();

  _entities.clear();
  _playerEntity.reset();
}

void GameScene::launchBullet(rtk::EngineContext &context) {
  std::size_t enttPlayer = 0;

  if (_playerEntity.has_value())
    enttPlayer = _playerEntity.value();

  const auto entity = context.registry.spawn_entity();

  rtk::ecs::SparseArray<component::Transform> &TransformSparseArray =
      context.registry.get_components<component::Transform>();

  component::Transform transform = {};
  transform.position = {TransformSparseArray.get(enttPlayer)->position};
  transform.rotation = 0.f;
  transform.scale = {1.f, 1.f};

  rtk::ecs::concepts::SpriteData sprite{};
  sprite.size = {32.f, 16.f};
  sprite.origin = {0.f, 0.f};
  sprite.textureRect = {0, 0, 32, 16};
  sprite.color = {255, 255, 255, 255};
  sprite.textureId = _textureId;

  component::AABBCollider collider{};
  collider.offsetX = 0.f;
  collider.offsetY = 0.f;
  collider.width = 32.f;
  collider.height = 16.f;
  collider.layer = Collider::CollisionLayer::PlayerProjectile;
  collider.mask = Collider::CollisionLayer::EnemyProjectile |
                  Collider::CollisionLayer::Enemy;

  context.registry.get_components<component::Transform>().insert_at(entity,
                                                                    transform);
  context.registry.get_components<rtk::ecs::concepts::SpriteData>().insert_at(
      entity, sprite);
  context.registry.get_components<component::AABBCollider>().insert_at(
      entity, collider);

  for (int i = 0; i != bullets.size(); i++) {
    if (bullets[i] == 0) {
      bullets[i] = entity;
      return;
    }
  }
}
} // namespace rtype::client