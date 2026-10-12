#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "EngineContext.hpp"
#include "graphical/renderWindow.hpp"
#include "sceneManager/sceneConcept.hpp"

#include "components/AABBCollider.hpp"
#include "components/Controllable.hpp"
#include "components/Transform.hpp"
#include "components/Velocity.hpp"

#include "systems/AnimationSystem.hpp"
#include "systems/RenderConcept.hpp"

#include "sceneType.hpp"

namespace rtype::client {
class GameScene {
public:
  explicit GameScene(rtk::RenderWindow &window);
  ~GameScene() = default;

  static constexpr SceneType Type = SceneType::Game;

  void onEnter(rtk::EngineContext &context);
  std::optional<SceneType> onUpdate(rtk::EngineContext &context,
                                    const rtk::InputState &inputState,
                                    float dt);
  void onExit(rtk::EngineContext &context);

private:
  void createEnemy(rtk::EngineContext &context);

  float createHighGround(rtk::EngineContext &context, float startX) {
    const auto highGroundEntity = context.registry.spawn_entity();
    _backgroundTextureId = _window.loadTexture("stage1.png").getHandle();

    component::Transform highGroundTransform{};
    highGroundTransform.position = {startX, 624.0f};
    highGroundTransform.rotation = 0.0f;
    highGroundTransform.scale = {2.0f, 2.0f};

    rtk::ecs::concepts::SpriteData highGroundSprite{};
    highGroundSprite.size = {64.f, 48.f};
    highGroundSprite.origin = {0.f, 0.0f};
    highGroundSprite.textureRect = {320, 192, 64, 48};
    highGroundSprite.color = {255, 255, 255, 255};
    highGroundSprite.textureId = _backgroundTextureId;
    highGroundSprite.layer = -1;

    component::AABBCollider highGroundCollider{};
    highGroundCollider.offsetX = 0.0f;
    highGroundCollider.offsetY = 0.0f;
    highGroundCollider.width = 64.0f;
    highGroundCollider.height = 48.0f;
    highGroundCollider.layer = Collider::CollisionLayer::Environment;
    highGroundCollider.mask = Collider::CollisionLayer::Player |
                              Collider::CollisionLayer::Enemy |
                              Collider::CollisionLayer::PlayerProjectile |
                              Collider::CollisionLayer::EnemyProjectile;

    component::Velocity highGroundVelocity{};
    highGroundVelocity.x = -50.0f;
    highGroundVelocity.y = 0.f;

    context.registry.get_components<component::Transform>().insert_at(
        highGroundEntity, highGroundTransform);
    context.registry.get_components<rtk::ecs::concepts::SpriteData>().insert_at(
        highGroundEntity, highGroundSprite);
    context.registry.get_components<component::AABBCollider>().insert_at(
        highGroundEntity, highGroundCollider);
    context.registry.get_components<component::Velocity>().insert_at(
        highGroundEntity, highGroundVelocity);

    _entities.push_back(highGroundEntity);
    return startX + (64.0f * 2.0f);
  }

  float createLowGround(rtk::EngineContext &context, float startX) {
    const auto lowGroundEntity = context.registry.spawn_entity();
    _backgroundTextureId = _window.loadTexture("stage1.png").getHandle();

    component::Transform lowGroundTransform{};
    lowGroundTransform.position = {startX, 688.0f};
    lowGroundTransform.rotation = 0.0f;
    lowGroundTransform.scale = {2.0f, 2.0f};

    rtk::ecs::concepts::SpriteData lowGroundSprite{};
    lowGroundSprite.size = {256.0f, 16.0f};
    lowGroundSprite.origin = {0.0f, 0.0f};
    lowGroundSprite.textureRect = {64, 224, 256, 16};
    lowGroundSprite.color = {255, 255, 255, 255};
    lowGroundSprite.textureId = _backgroundTextureId;
    lowGroundSprite.layer = -1;

    component::AABBCollider lowGroundCollider{};
    lowGroundCollider.offsetX = 0.0f;
    lowGroundCollider.offsetY = 0.0f;
    lowGroundCollider.width = 256.0f;
    lowGroundCollider.height = 16.0f;
    lowGroundCollider.layer = Collider::CollisionLayer::Environment;
    lowGroundCollider.mask = Collider::CollisionLayer::Player |
                             Collider::CollisionLayer::Enemy |
                             Collider::CollisionLayer::PlayerProjectile |
                             Collider::CollisionLayer::EnemyProjectile;

    component::Velocity lowGroundVelocity{};
    lowGroundVelocity.x = -50.0f;
    lowGroundVelocity.y = 0.0f;

    context.registry.get_components<component::Transform>().insert_at(
        lowGroundEntity, lowGroundTransform);
    context.registry.get_components<rtk::ecs::concepts::SpriteData>().insert_at(
        lowGroundEntity, lowGroundSprite);
    context.registry.get_components<component::AABBCollider>().insert_at(
        lowGroundEntity, lowGroundCollider);
    context.registry.get_components<component::Velocity>().insert_at(
        lowGroundEntity, lowGroundVelocity);

    _entities.push_back(lowGroundEntity);
    return startX + (256.0f * 2.0f);
  }

  float createMediumGround(rtk::EngineContext &context, float startX) {
    const auto groundEntity = context.registry.spawn_entity();
    _backgroundTextureId = _window.loadTexture("stage1.png").getHandle();

    component::Transform groundTransform{};
    groundTransform.position = {startX, 656.0f};
    groundTransform.rotation = 0.0f;
    groundTransform.scale = {2.0f, 2.0f};

    rtk::ecs::concepts::SpriteData groundSprite{};
    groundSprite.size = {64.f, 32.f};
    groundSprite.origin = {0.0f, 0.0f};
    groundSprite.textureRect = {0, 208, 64, 32};
    groundSprite.color = {255, 255, 255, 255};
    groundSprite.textureId = _backgroundTextureId;
    groundSprite.layer = -1;

    component::AABBCollider groundCollider{};
    groundCollider.offsetX = 0.0f;
    groundCollider.offsetY = 0.0f;
    groundCollider.width = 64.0f;
    groundCollider.height = 32.0f;

    groundCollider.layer = Collider::CollisionLayer::Environment;
    groundCollider.mask = Collider::CollisionLayer::Player |
                          Collider::CollisionLayer::Enemy |
                          Collider::CollisionLayer::PlayerProjectile |
                          Collider::CollisionLayer::EnemyProjectile;

    component::Velocity groundVelocity{};
    groundVelocity.x = -50.0f;
    groundVelocity.y = 0.f;

    context.registry.get_components<component::Transform>().insert_at(
        groundEntity, groundTransform);
    context.registry.get_components<rtk::ecs::concepts::SpriteData>().insert_at(
        groundEntity, groundSprite);
    context.registry.get_components<component::AABBCollider>().insert_at(
        groundEntity, groundCollider);
    context.registry.get_components<component::Velocity>().insert_at(
        groundEntity, groundVelocity);

    _entities.push_back(groundEntity);
    return startX + (64.0f * 2.0f);
  }

  void createBackground(rtk::EngineContext &context) {
    float currentX = 0.0f;
    currentX = createMediumGround((context), currentX);
    currentX = createLowGround((context), currentX);
    currentX = createHighGround((context), currentX);
    currentX = createMediumGround((context), currentX);
    currentX = createLowGround((context), currentX);
    currentX = createHighGround((context), currentX);
    currentX = createHighGround((context), currentX);
    currentX = createMediumGround((context), currentX);
    currentX = createLowGround((context), currentX);
    currentX = createMediumGround((context), currentX);
  }

  void createPlayer(rtk::EngineContext &context) {
    const auto entity = context.registry.spawn_entity();
    _textureId = _window.loadTexture("sprite.png").getHandle();

    component::Transform transform{};
    transform.position = {150.f, 250.f};
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
    collider.layer = Collider::CollisionLayer::Player;
    collider.mask = Collider::CollisionLayer::Enemy |
                    Collider::CollisionLayer::EnemyProjectile |
                    Collider::CollisionLayer::Environment;

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

    context.registry.get_components<component::Controllable>().insert_at(entity,
                                                                         {});
    context.registry.get_components<component::Velocity>().insert_at(entity,
                                                                     {});

    _entities.push_back(entity);
  }
  rtk::RenderWindow &_window;

  std::vector<std::size_t> _entities;
  std::optional<std::size_t> _playerEntity;

  std::vector<std::size_t> bullets;

  std::uint32_t _textureId = 0;
  std::uint32_t _backgroundTextureId = 0;

  std::uint16_t _bulletPressedByTick = 0;
};
} // namespace rtype::client