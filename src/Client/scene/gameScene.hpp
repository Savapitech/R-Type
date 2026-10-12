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

  float createGate(rtk::EngineContext &context, float startX, bool symmetric) {
    const auto gateEntity = context.registry.spawn_entity();
    _backgroundTextureId = _window.loadTexture("stage1.png").getHandle();

    component::Transform gateTransform{};
    gateTransform.position = {startX, 528.0f};
    gateTransform.rotation = 0.0f;
    gateTransform.scale = {2.0f, 2.0f};

    rtk::ecs::concepts::SpriteData gateSprite{};
    gateSprite.size = {192.0f, 96.0f};
    gateSprite.origin = {0.0f, 0.0f};
    gateSprite.textureRect = {3008, 144, 192, 96};
    gateSprite.color = {255, 255, 255, 255};
    gateSprite.textureId = _backgroundTextureId;
    gateSprite.layer = -1;

    component::AABBCollider gateCollider{};
    gateCollider.offsetX = 0.0f;
    gateCollider.offsetY = 0.0f;
    gateCollider.width = 192.0f;
    gateCollider.height = 96.0f;
    gateCollider.layer = Collider::CollisionLayer::Environment;
    gateCollider.mask = Collider::CollisionLayer::Player |
                        Collider::CollisionLayer::Enemy |
                        Collider::CollisionLayer::PlayerProjectile |
                        Collider::CollisionLayer::EnemyProjectile;

    component::Velocity gateVelocity{};
    gateVelocity.x = -50.0f;
    gateVelocity.y = 0.0f;

    context.registry.get_components<component::Transform>().insert_at(
        gateEntity, gateTransform);
    context.registry.get_components<rtk::ecs::concepts::SpriteData>().insert_at(
        gateEntity, gateSprite);
    context.registry.get_components<component::AABBCollider>().insert_at(
        gateEntity, gateCollider);
    context.registry.get_components<component::Velocity>().insert_at(
        gateEntity, gateVelocity);

    _entities.push_back(gateEntity);

    if (symmetric) {
      const auto ceilingEntity = context.registry.spawn_entity();
      component::Transform ceilingTransform{};
      ceilingTransform.position = {startX, 0.0f};
      ceilingTransform.rotation = 0.0f;
      ceilingTransform.scale = {2.0f, 2.0f};
      rtk::ecs::concepts::SpriteData ceilingSprite = gateSprite;
      ceilingSprite.flags = 1 << 1;

      component::AABBCollider ceilingCollider = gateCollider;

      component::Velocity ceilingVelocity = gateVelocity;

      context.registry.get_components<component::Transform>().insert_at(
          ceilingEntity, ceilingTransform);
      context.registry.get_components<rtk::ecs::concepts::SpriteData>()
          .insert_at(ceilingEntity, ceilingSprite);
      context.registry.get_components<component::AABBCollider>().insert_at(
          ceilingEntity, ceilingCollider);
      context.registry.get_components<component::Velocity>().insert_at(
          ceilingEntity, ceilingVelocity);

      _entities.push_back(ceilingEntity);
    }
    return startX + (192.0f * 2.0f);
  }
  float createRamp(rtk::EngineContext &context, float startX, bool symmetric) {
    const auto rampEntity = context.registry.spawn_entity();
    _backgroundTextureId = _window.loadTexture("stage1.png").getHandle();

    component::Transform rampTransform{};
    rampTransform.position = {startX, 640.0f};
    rampTransform.rotation = 0.0f;
    rampTransform.scale = {2.0f, 2.0f};

    rtk::ecs::concepts::SpriteData rampSprite{};
    rampSprite.size = {192.0f, 40.0f};
    rampSprite.origin = {0.0f, 0.0f};
    rampSprite.textureRect = {2240, 200, 192, 40};
    rampSprite.color = {255, 255, 255, 255};
    rampSprite.textureId = _backgroundTextureId;
    rampSprite.layer = -1;

    component::AABBCollider rampCollider{};
    rampCollider.offsetX = 0.0f;
    rampCollider.offsetY = 0.0f;
    rampCollider.width = 192.0f;
    rampCollider.height = 40.0f;
    rampCollider.layer = Collider::CollisionLayer::Environment;
    rampCollider.mask = Collider::CollisionLayer::Player |
                        Collider::CollisionLayer::Enemy |
                        Collider::CollisionLayer::PlayerProjectile |
                        Collider::CollisionLayer::EnemyProjectile;

    component::Velocity rampVelocity{};
    rampVelocity.x = -50.0f;
    rampVelocity.y = 0.0f;

    context.registry.get_components<component::Transform>().insert_at(
        rampEntity, rampTransform);
    context.registry.get_components<rtk::ecs::concepts::SpriteData>().insert_at(
        rampEntity, rampSprite);
    context.registry.get_components<component::AABBCollider>().insert_at(
        rampEntity, rampCollider);
    context.registry.get_components<component::Velocity>().insert_at(
        rampEntity, rampVelocity);

    _entities.push_back(rampEntity);

    if (symmetric) {
      const auto ceilingEntity = context.registry.spawn_entity();
      component::Transform ceilingTransform{};
      ceilingTransform.position = {startX, 0.0f};
      ceilingTransform.rotation = 0.0f;
      ceilingTransform.scale = {2.0f, 2.0f};
      rtk::ecs::concepts::SpriteData ceilingSprite = rampSprite;
      ceilingSprite.flags = 1 << 1;

      component::AABBCollider ceilingCollider = rampCollider;

      component::Velocity ceilingVelocity = rampVelocity;

      context.registry.get_components<component::Transform>().insert_at(
          ceilingEntity, ceilingTransform);
      context.registry.get_components<rtk::ecs::concepts::SpriteData>()
          .insert_at(ceilingEntity, ceilingSprite);
      context.registry.get_components<component::AABBCollider>().insert_at(
          ceilingEntity, ceilingCollider);
      context.registry.get_components<component::Velocity>().insert_at(
          ceilingEntity, ceilingVelocity);

      _entities.push_back(ceilingEntity);
    }
    return startX + (192.0f * 2.0f);
  }
  float createReactor(rtk::EngineContext &context, float startX,
                      bool symmetric) {
    const auto reactorEntity = context.registry.spawn_entity();
    _backgroundTextureId = _window.loadTexture("stage1.png").getHandle();

    component::Transform reactorTransform{};
    reactorTransform.position = {startX, 526.0f};
    reactorTransform.rotation = 0.0f;
    reactorTransform.scale = {2.0f, 2.0f};

    rtk::ecs::concepts::SpriteData reactorSprite{};
    reactorSprite.size = {113.0f, 97.0f};
    reactorSprite.origin = {0.0f, 0.0f};
    reactorSprite.textureRect = {1552, 144, 113, 97};
    reactorSprite.color = {255, 255, 255, 255};
    reactorSprite.textureId = _backgroundTextureId;
    reactorSprite.layer = -1;

    component::AABBCollider reactorCollider{};
    reactorCollider.offsetX = 0.0f;
    reactorCollider.offsetY = 0.0f;
    reactorCollider.width = 113.0f;
    reactorCollider.height = 97.0f;
    reactorCollider.layer = Collider::CollisionLayer::Environment;
    reactorCollider.mask = Collider::CollisionLayer::Player |
                           Collider::CollisionLayer::Enemy |
                           Collider::CollisionLayer::PlayerProjectile |
                           Collider::CollisionLayer::EnemyProjectile;

    component::Velocity reactorVelocity{};
    reactorVelocity.x = -50.0f;
    reactorVelocity.y = 0.0f;

    context.registry.get_components<component::Transform>().insert_at(
        reactorEntity, reactorTransform);
    context.registry.get_components<rtk::ecs::concepts::SpriteData>().insert_at(
        reactorEntity, reactorSprite);
    context.registry.get_components<component::AABBCollider>().insert_at(
        reactorEntity, reactorCollider);
    context.registry.get_components<component::Velocity>().insert_at(
        reactorEntity, reactorVelocity);

    _entities.push_back(reactorEntity);
    if (symmetric) {
      const auto ceilingEntity = context.registry.spawn_entity();
      component::Transform ceilingTransform{};
      ceilingTransform.position = {startX, 0.0f};
      ceilingTransform.rotation = 0.0f;
      ceilingTransform.scale = {2.0f, 2.0f};
      rtk::ecs::concepts::SpriteData ceilingSprite = reactorSprite;
      ceilingSprite.flags = 1 << 1;

      component::AABBCollider ceilingCollider = reactorCollider;

      component::Velocity ceilingVelocity = reactorVelocity;

      context.registry.get_components<component::Transform>().insert_at(
          ceilingEntity, ceilingTransform);
      context.registry.get_components<rtk::ecs::concepts::SpriteData>()
          .insert_at(ceilingEntity, ceilingSprite);
      context.registry.get_components<component::AABBCollider>().insert_at(
          ceilingEntity, ceilingCollider);
      context.registry.get_components<component::Velocity>().insert_at(
          ceilingEntity, ceilingVelocity);

      _entities.push_back(ceilingEntity);
    }
    return startX + (113.0f * 2.0f);
  }

  float createHighGround(rtk::EngineContext &context, float startX,
                         bool symmetric) {
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
    if (symmetric) {
      const auto ceilingEntity = context.registry.spawn_entity();
      component::Transform ceilingTransform{};
      ceilingTransform.position = {startX, 0.0f};
      ceilingTransform.rotation = 0.0f;
      ceilingTransform.scale = {2.0f, 2.0f};
      rtk::ecs::concepts::SpriteData ceilingSprite = highGroundSprite;
      ceilingSprite.flags = 1 << 1;

      component::AABBCollider ceilingCollider = highGroundCollider;

      component::Velocity ceilingVelocity = highGroundVelocity;

      context.registry.get_components<component::Transform>().insert_at(
          ceilingEntity, ceilingTransform);
      context.registry.get_components<rtk::ecs::concepts::SpriteData>()
          .insert_at(ceilingEntity, ceilingSprite);
      context.registry.get_components<component::AABBCollider>().insert_at(
          ceilingEntity, ceilingCollider);
      context.registry.get_components<component::Velocity>().insert_at(
          ceilingEntity, ceilingVelocity);

      _entities.push_back(ceilingEntity);
    }
    return startX + (64.0f * 2.0f);
  }

  float createLowGround(rtk::EngineContext &context, float startX,
                        bool symmetric) {
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

    if (symmetric) {
      const auto ceilingEntity = context.registry.spawn_entity();
      component::Transform ceilingTransform{};
      ceilingTransform.position = {startX, 0.0f};
      ceilingTransform.rotation = 0.0f;
      ceilingTransform.scale = {2.0f, 2.0f};
      rtk::ecs::concepts::SpriteData ceilingSprite = lowGroundSprite;
      ceilingSprite.flags = 1 << 1;

      component::AABBCollider ceilingCollider = lowGroundCollider;

      component::Velocity ceilingVelocity = lowGroundVelocity;

      context.registry.get_components<component::Transform>().insert_at(
          ceilingEntity, ceilingTransform);
      context.registry.get_components<rtk::ecs::concepts::SpriteData>()
          .insert_at(ceilingEntity, ceilingSprite);
      context.registry.get_components<component::AABBCollider>().insert_at(
          ceilingEntity, ceilingCollider);
      context.registry.get_components<component::Velocity>().insert_at(
          ceilingEntity, ceilingVelocity);

      _entities.push_back(ceilingEntity);
    }

    return startX + (256.0f * 2.0f);
  }

  float createMediumGround(rtk::EngineContext &context, float startX,
                           bool symmetric) {
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

    if (symmetric) {
      const auto ceilingEntity = context.registry.spawn_entity();
      component::Transform ceilingTransform{};
      ceilingTransform.position = {startX, 0.0f};
      ceilingTransform.rotation = 0.0f;
      ceilingTransform.scale = {2.0f, 2.0f};
      rtk::ecs::concepts::SpriteData ceilingSprite = groundSprite;
      ceilingSprite.flags = 1 << 1;

      component::AABBCollider ceilingCollider = groundCollider;

      component::Velocity ceilingVelocity = groundVelocity;

      context.registry.get_components<component::Transform>().insert_at(
          ceilingEntity, ceilingTransform);
      context.registry.get_components<rtk::ecs::concepts::SpriteData>()
          .insert_at(ceilingEntity, ceilingSprite);
      context.registry.get_components<component::AABBCollider>().insert_at(
          ceilingEntity, ceilingCollider);
      context.registry.get_components<component::Velocity>().insert_at(
          ceilingEntity, ceilingVelocity);

      _entities.push_back(ceilingEntity);
    }

    return startX + (64.0f * 2.0f);
  }

  void createBackground(rtk::EngineContext &context) {
    float currentX = 0.0f;
    currentX = createMediumGround((context), currentX, false);
    currentX = createLowGround((context), currentX, false);
    currentX = createHighGround((context), currentX, false);
    currentX = createMediumGround((context), currentX, false);
    currentX = createLowGround((context), currentX, false);
    currentX = createHighGround((context), currentX, false);
    currentX = createHighGround((context), currentX, false);
    currentX = createMediumGround((context), currentX, false);
    currentX = createLowGround((context), currentX, false);
    currentX = createMediumGround((context), currentX, false);
    currentX = createReactor(context, currentX, true);
    currentX = createLowGround((context), currentX, true);
    currentX = createMediumGround((context), currentX, true);
    currentX = createHighGround((context), currentX, true);
    currentX = createLowGround((context), currentX, true);
    currentX = createRamp((context), currentX, true);
    currentX = createMediumGround((context), currentX, true);
    currentX = createMediumGround((context), currentX, true);
    currentX = createMediumGround((context), currentX, true);
    currentX = createHighGround((context), currentX, true);
    currentX = createGate((context), currentX, true);
    currentX = createLowGround((context), currentX, true);
    currentX = createMediumGround((context), currentX, true);
    currentX = createHighGround((context), currentX, true);
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