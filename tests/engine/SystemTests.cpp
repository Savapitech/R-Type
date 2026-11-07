#include "Test.hpp"

#include <Registry.hpp>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "collision/collisionBuffer.hpp"
#include "components/AABBCollider.hpp"
#include "components/Animation.hpp"
#include "components/Animator.hpp"
#include "components/AttachedTo.hpp"
#include "components/Dead.hpp"
#include "components/DestroyAnimEnd.hpp"
#include "components/LuaScript.hpp"
#include "components/Parallax.hpp"
#include "components/Transform.hpp"
#include "components/Velocity.hpp"
#include "events/busEvents.hpp"
#include "systems/AnimationCleanSystem.hpp"
#include "systems/AnimationSystem.hpp"
#include "systems/AnimatorSystem.hpp"
#include "systems/AttachmentSystem.hpp"
#include "systems/AudioSystem.hpp"
#include "systems/ColliderSystem.hpp"
#include "systems/DeadSystem.hpp"
#include "systems/LuaScriptSystem.hpp"
#include "systems/MovementSystem.hpp"
#include "systems/ParallaxSystem.hpp"

using component::AABBCollider;
using component::Transform;
using component::Velocity;
using rtk::ecs::Registry;
using Sprite = rtk::ecs::concepts::SpriteData;

namespace {

Transform at(float x, float y, float scale = 1.0f) {
  return Transform{0.0f, {x, y}, {scale, scale}};
}

AABBCollider box(float width, float height,
                 std::uint32_t layer = Collider::Player,
                 std::uint32_t mask = 0xFFFFFFFF) {
  AABBCollider collider;
  collider.width = width;
  collider.height = height;
  collider.layer = layer;
  collider.mask = mask;
  return collider;
}

/// Registry and systems used to test collisions.
struct CollisionWorld {
  Registry registry;
  busEvent bus;
  rtk::collision::CollisionBuffer buffer;
  rtk::systems::ColliderSystem system{buffer, 64.0f, bus};

  CollisionWorld() {
    registry.register_component<Transform>();
    registry.register_component<AABBCollider>();
  }

  std::size_t spawn(const Transform &transform, const AABBCollider &collider) {
    const auto entity = registry.spawn_entity();
    registry.get_components<Transform>().insert_at(entity, transform);
    registry.get_components<AABBCollider>().insert_at(entity, collider);
    return entity;
  }

  const std::vector<rtk::collision::Collision> &run() {
    system.update(registry, 0.016f);
    return buffer.collisions();
  }
};

/// Registry with an entity holding an Animation and a sprite.
struct AnimationWorld {
  Registry registry;
  std::size_t entity;
  rtk::systems::AnimationSystem system;

  explicit AnimationWorld(const component::Animation &animation) {
    registry.register_component<component::Animation>();
    registry.register_component<Sprite>();
    entity = registry.spawn_entity();
    registry.get_components<component::Animation>().insert_at(entity,
                                                              animation);
    registry.get_components<Sprite>().insert_at(entity, Sprite{});
  }

  component::Animation &animation() {
    return *registry.get_components<component::Animation>().get(entity);
  }
  Sprite &sprite() { return *registry.get_components<Sprite>().get(entity); }
  void run(float dt) { system.update(registry, dt); }
};

component::Animation strip(std::uint16_t frames, float duration = 0.25f) {
  component::Animation animation;
  animation.totalFrames = frames;
  animation.frameDuration = duration;
  animation.frameWidth = 32;
  animation.frameHeight = 16;
  return animation;
}

} // namespace

// MovementSystem

TEST(MovementSystem, AppliesVelocityTimesDelta) {
  Registry registry;
  registry.register_component<Transform>();
  registry.register_component<Velocity>();
  const auto entity = registry.spawn_entity();
  registry.get_components<Transform>().insert_at(entity, at(10, 20));
  registry.get_components<Velocity>().insert_at(entity, Velocity{100, -50});

  rtk::systems::MovementSystem system;
  system.update(registry, 0.5f);
  const auto *transform = registry.get_components<Transform>().get(entity);
  CHECK_EQ(transform->position.x, 60.0f);
  CHECK_EQ(transform->position.y, -5.0f);
}

TEST(MovementSystem, IgnoresEntitiesWithoutVelocity) {
  Registry registry;
  registry.register_component<Transform>();
  registry.register_component<Velocity>();
  const auto entity = registry.spawn_entity();
  registry.get_components<Transform>().insert_at(entity, at(3, 4));

  rtk::systems::MovementSystem system;
  system.update(registry, 1.0f);
  CHECK_EQ(registry.get_components<Transform>().get(entity)->position.x, 3.0f);
}

TEST(MovementSystem, ZeroDeltaDoesNotMove) {
  Registry registry;
  registry.register_component<Transform>();
  registry.register_component<Velocity>();
  const auto entity = registry.spawn_entity();
  registry.get_components<Transform>().insert_at(entity, at(3, 4));
  registry.get_components<Velocity>().insert_at(entity, Velocity{999, 999});

  rtk::systems::MovementSystem system;
  system.update(registry, 0.0f);
  CHECK_EQ(registry.get_components<Transform>().get(entity)->position.y, 4.0f);
}

TEST(MovementSystem, MovesEveryEntity) {
  Registry registry;
  registry.register_component<Transform>();
  registry.register_component<Velocity>();
  std::vector<std::size_t> entities;
  for (int i = 0; i < 10; ++i) {
    const auto entity = registry.spawn_entity();
    registry.get_components<Transform>().insert_at(entity, at(0, 0));
    registry.get_components<Velocity>().insert_at(
        entity, Velocity{static_cast<float>(i), 0});
    entities.push_back(entity);
  }

  rtk::systems::MovementSystem system;
  system.update(registry, 2.0f);
  for (int i = 0; i < 10; ++i)
    CHECK_EQ(registry.get_components<Transform>().get(entities[i])->position.x,
             2.0f * i);
}

TEST(MovementSystem, AccumulatesOverFrames) {
  Registry registry;
  registry.register_component<Transform>();
  registry.register_component<Velocity>();
  const auto entity = registry.spawn_entity();
  registry.get_components<Transform>().insert_at(entity, at(0, 0));
  registry.get_components<Velocity>().insert_at(entity, Velocity{0, 8});

  rtk::systems::MovementSystem system;
  for (int i = 0; i < 4; ++i)
    system.update(registry, 0.25f);
  CHECK_EQ(registry.get_components<Transform>().get(entity)->position.y, 8.0f);
}

TEST(MovementSystem, KeepsScaleAndRotation) {
  Registry registry;
  registry.register_component<Transform>();
  registry.register_component<Velocity>();
  const auto entity = registry.spawn_entity();
  registry.get_components<Transform>().insert_at(
      entity, Transform{1.0f, {0, 0}, {2, 3}});
  registry.get_components<Velocity>().insert_at(entity, Velocity{1, 1});

  rtk::systems::MovementSystem system;
  system.update(registry, 1.0f);
  const auto *transform = registry.get_components<Transform>().get(entity);
  CHECK_EQ(transform->rotation, 1.0f);
  CHECK_EQ(transform->scale.x, 2.0f);
  CHECK_EQ(transform->scale.y, 3.0f);
}

// ColliderSystem

TEST(ColliderSystem, OverlappingBoxesCollide) {
  CollisionWorld world;
  const auto first = world.spawn(at(0, 0), box(10, 10));
  const auto second = world.spawn(at(5, 5), box(10, 10));

  const auto &collisions = world.run();
  CHECK_EQ(collisions.size(), 1u);
  CHECK_EQ(collisions[0].firstEntity, first);
  CHECK_EQ(collisions[0].secondEntity, second);
}

TEST(ColliderSystem, SeparateBoxesDoNotCollide) {
  CollisionWorld world;
  world.spawn(at(0, 0), box(10, 10));
  world.spawn(at(20, 0), box(10, 10));

  CHECK(world.run().empty());
}

TEST(ColliderSystem, TouchingEdgesDoNotCollide) {
  CollisionWorld world;
  world.spawn(at(0, 0), box(10, 10));
  world.spawn(at(10, 0), box(10, 10));

  CHECK(world.run().empty());
}

TEST(ColliderSystem, LowerEntityIsAlwaysFirst) {
  CollisionWorld world;
  const auto first = world.spawn(at(100, 100), box(10, 10));
  const auto second = world.spawn(at(95, 95), box(10, 10));

  const auto &collisions = world.run();
  CHECK_EQ(collisions.size(), 1u);
  CHECK(collisions[0].firstEntity < collisions[0].secondEntity);
  CHECK_EQ(collisions[0].firstEntity, first);
  CHECK_EQ(collisions[0].secondEntity, second);
}

TEST(ColliderSystem, MaskFiltersLayers) {
  CollisionWorld world;
  world.spawn(at(0, 0), box(10, 10, Collider::Player, Collider::Enemy));
  world.spawn(at(5, 0), box(10, 10, Collider::PowerUp, 0xFFFFFFFF));

  CHECK(world.run().empty());
}

TEST(ColliderSystem, MaskMustMatchBothWays) {
  CollisionWorld world;
  world.spawn(at(0, 0), box(10, 10, Collider::Player, Collider::Enemy));
  world.spawn(at(5, 0), box(10, 10, Collider::Enemy, Collider::Environment));

  CHECK(world.run().empty());
}

TEST(ColliderSystem, MatchingMasksCollide) {
  CollisionWorld world;
  world.spawn(at(0, 0),
              box(10, 10, Collider::PlayerProjectile, Collider::Enemy));
  world.spawn(at(5, 0),
              box(10, 10, Collider::Enemy, Collider::PlayerProjectile));

  CHECK_EQ(world.run().size(), 1u);
}

TEST(ColliderSystem, EmptyBoxIsIgnored) {
  CollisionWorld world;
  world.spawn(at(0, 0), box(0, 10));
  world.spawn(at(0, 0), box(10, 10));

  CHECK(world.run().empty());
}

TEST(ColliderSystem, ScaleGrowsTheBox) {
  CollisionWorld world;
  world.spawn(at(0, 0, 3.0f), box(10, 10));
  world.spawn(at(25, 25), box(10, 10));

  CHECK_EQ(world.run().size(), 1u);
}

TEST(ColliderSystem, OffsetMovesTheBox) {
  CollisionWorld world;
  auto shifted = box(10, 10);
  shifted.offsetX = 50;
  world.spawn(at(0, 0), shifted);
  world.spawn(at(55, 0), box(10, 10));

  CHECK_EQ(world.run().size(), 1u);
}

TEST(ColliderSystem, NegativeScaleFlipsTheBox) {
  CollisionWorld world;
  world.spawn(at(20, 0, -1.0f), box(10, 10));
  world.spawn(at(12, -5), box(4, 4));

  CHECK_EQ(world.run().size(), 1u);
}

TEST(ColliderSystem, WorksWithNegativeCoordinates) {
  CollisionWorld world;
  world.spawn(at(-200, -200), box(30, 30));
  world.spawn(at(-180, -190), box(30, 30));

  CHECK_EQ(world.run().size(), 1u);
}

TEST(ColliderSystem, BigBoxesAreReportedOnce) {
  CollisionWorld world;
  world.spawn(at(0, 0), box(500, 500));
  world.spawn(at(10, 10), box(500, 500));

  CHECK_EQ(world.run().size(), 1u);
}

TEST(ColliderSystem, FindsEveryPair) {
  CollisionWorld world;
  world.spawn(at(0, 0), box(10, 10));
  world.spawn(at(2, 2), box(10, 10));
  world.spawn(at(4, 4), box(10, 10));

  CHECK_EQ(world.run().size(), 3u);
}

TEST(ColliderSystem, PublishesOnTheEventBus) {
  CollisionWorld world;
  std::vector<rtk::collision::Collision> published;
  world.bus.subscribe<rtk::collision::Collision>(
      [&](const rtk::collision::Collision &collision) {
        published.push_back(collision);
      });
  const auto first = world.spawn(at(0, 0), box(10, 10));
  const auto second = world.spawn(at(1, 1), box(10, 10));

  world.run();
  CHECK_EQ(published.size(), 1u);
  CHECK_EQ(published[0].firstEntity, first);
  CHECK_EQ(published[0].secondEntity, second);
}

TEST(ColliderSystem, BufferIsClearedEveryUpdate) {
  CollisionWorld world;
  const auto mover = world.spawn(at(0, 0), box(10, 10));
  world.spawn(at(5, 5), box(10, 10));

  CHECK_EQ(world.run().size(), 1u);
  world.registry.get_components<Transform>().get(mover)->position.x = 500;
  CHECK(world.run().empty());
}

TEST(ColliderSystem, StopClearsTheBuffer) {
  CollisionWorld world;
  world.spawn(at(0, 0), box(10, 10));
  world.spawn(at(5, 5), box(10, 10));

  world.run();
  world.system.onStop(world.registry);
  CHECK(world.buffer.collisions().empty());
}

TEST(ColliderSystem, EntityWithoutTransformIsIgnored) {
  CollisionWorld world;
  world.spawn(at(0, 0), box(10, 10));
  const auto ghost = world.registry.spawn_entity();
  world.registry.get_components<AABBCollider>().insert_at(ghost, box(10, 10));

  CHECK(world.run().empty());
}

// AttachmentSystem

TEST(AttachmentSystem, FollowsTheTargetWithOffset) {
  Registry registry;
  registry.register_component<Transform>();
  registry.register_component<component::AttachedTo>();
  const auto target = registry.spawn_entity();
  const auto child = registry.spawn_entity();
  registry.get_components<Transform>().insert_at(target, at(100, 50));
  registry.get_components<Transform>().insert_at(child, at(0, 0));
  registry.get_components<component::AttachedTo>().insert_at(
      child, component::AttachedTo{target, 10, -5});

  rtk::systems::AttachmentSystem system;
  system.update(registry, 0.016f);
  const auto *transform = registry.get_components<Transform>().get(child);
  CHECK_EQ(transform->position.x, 110.0f);
  CHECK_EQ(transform->position.y, 45.0f);
}

TEST(AttachmentSystem, TracksAMovingTarget) {
  Registry registry;
  registry.register_component<Transform>();
  registry.register_component<component::AttachedTo>();
  const auto target = registry.spawn_entity();
  const auto child = registry.spawn_entity();
  registry.get_components<Transform>().insert_at(target, at(0, 0));
  registry.get_components<Transform>().insert_at(child, at(0, 0));
  registry.get_components<component::AttachedTo>().insert_at(
      child, component::AttachedTo{target, 1, 1});

  rtk::systems::AttachmentSystem system;
  system.update(registry, 0.016f);
  registry.get_components<Transform>().get(target)->position = {30, 40};
  system.update(registry, 0.016f);
  CHECK_EQ(registry.get_components<Transform>().get(child)->position.x, 31.0f);
  CHECK_EQ(registry.get_components<Transform>().get(child)->position.y, 41.0f);
}

TEST(AttachmentSystem, OrphanIsDestroyed) {
  Registry registry;
  registry.register_component<Transform>();
  registry.register_component<component::AttachedTo>();
  const auto target = registry.spawn_entity();
  const auto child = registry.spawn_entity();
  registry.get_components<Transform>().insert_at(child, at(0, 0));
  registry.get_components<component::AttachedTo>().insert_at(
      child, component::AttachedTo{target, 0, 0});

  rtk::systems::AttachmentSystem system;
  system.update(registry, 0.016f);
  CHECK_FALSE(registry.has_component<Transform>(child));
  CHECK_FALSE(registry.has_component<component::AttachedTo>(child));
}

TEST(AttachmentSystem, OrphanIdIsFreedOnFlush) {
  Registry registry;
  registry.register_component<Transform>();
  registry.register_component<component::AttachedTo>();
  const auto target = registry.spawn_entity();
  const auto child = registry.spawn_entity();
  registry.get_components<Transform>().insert_at(child, at(0, 0));
  registry.get_components<component::AttachedTo>().insert_at(
      child, component::AttachedTo{target + 100, 0, 0});

  rtk::systems::AttachmentSystem system;
  system.update(registry, 0.016f);
  registry.flush();
  CHECK_EQ(registry.spawn_entity(), child);
}

// DeathSystem

TEST(DeathSystem, RemovesDeadEntities) {
  Registry registry;
  registry.register_component<component::Dead>();
  registry.register_component<Transform>();
  const auto entity = registry.spawn_entity();
  registry.get_components<Transform>().insert_at(entity, at(1, 1));
  registry.get_components<component::Dead>().insert_at(entity, {});

  rtk::systems::DeathSystem system;
  system.update(registry, 0.016f);
  CHECK_FALSE(registry.has_component<Transform>(entity));
  CHECK_FALSE(registry.has_component<component::Dead>(entity));
}

TEST(DeathSystem, KeepsLivingEntities) {
  Registry registry;
  registry.register_component<component::Dead>();
  registry.register_component<Transform>();
  const auto alive = registry.spawn_entity();
  const auto dead = registry.spawn_entity();
  registry.get_components<Transform>().insert_at(alive, at(1, 1));
  registry.get_components<Transform>().insert_at(dead, at(2, 2));
  registry.get_components<component::Dead>().insert_at(dead, {});

  rtk::systems::DeathSystem system;
  system.update(registry, 0.016f);
  CHECK(registry.has_component<Transform>(alive));
  CHECK_EQ(registry.get_components<Transform>().get(alive)->position.x, 1.0f);
}

TEST(DeathSystem, RemovesEveryDeadEntity) {
  Registry registry;
  registry.register_component<component::Dead>();
  for (int i = 0; i < 5; ++i)
    registry.get_components<component::Dead>().insert_at(
        registry.spawn_entity(), {});

  rtk::systems::DeathSystem system;
  system.update(registry, 0.016f);
  CHECK_EQ(registry.get_components<component::Dead>().size(), 0u);
}

TEST(DeathSystem, IdIsReusedAfterFlush) {
  Registry registry;
  registry.register_component<component::Dead>();
  const auto entity = registry.spawn_entity();
  registry.get_components<component::Dead>().insert_at(entity, {});

  rtk::systems::DeathSystem system;
  system.update(registry, 0.016f);
  registry.flush();
  CHECK_EQ(registry.spawn_entity(), entity);
}

// AnimationSystem

TEST(AnimationSystem, WaitsForTheFrameDuration) {
  AnimationWorld world(strip(4));

  world.run(0.125f);
  CHECK_EQ(world.animation().currentFrame, 0);
  world.run(0.125f);
  CHECK_EQ(world.animation().currentFrame, 1);
}

TEST(AnimationSystem, SkipsSeveralFramesOnALongDelta) {
  AnimationWorld world(strip(8));

  world.run(0.75f);
  CHECK_EQ(world.animation().currentFrame, 3);
  CHECK_EQ(world.animation().elapsedTime, 0.0f);
}

TEST(AnimationSystem, LoopsBackToTheFirstFrame) {
  AnimationWorld world(strip(2));

  world.run(0.25f);
  world.run(0.25f);
  CHECK_EQ(world.animation().currentFrame, 0);
  CHECK(world.animation().playing);
}

TEST(AnimationSystem, StopsOnTheLastFrameWithoutLoop) {
  auto animation = strip(3);
  animation.loop = false;
  AnimationWorld world(animation);

  world.run(5.0f);
  CHECK_EQ(world.animation().currentFrame, 2);
  CHECK_FALSE(world.animation().playing);
}

TEST(AnimationSystem, PausedAnimationDoesNotAdvance) {
  auto animation = strip(4);
  animation.playing = false;
  AnimationWorld world(animation);

  world.run(1.0f);
  CHECK_EQ(world.animation().currentFrame, 0);
}

TEST(AnimationSystem, SingleFrameNeverAdvances) {
  AnimationWorld world(strip(1));

  world.run(10.0f);
  CHECK_EQ(world.animation().currentFrame, 0);
  CHECK_EQ(world.animation().elapsedTime, 0.0f);
}

TEST(AnimationSystem, TextureRectFollowsTheFrame) {
  auto animation = strip(4);
  animation.startX = 10;
  animation.startY = 20;
  animation.paddingX = 2;
  AnimationWorld world(animation);

  world.run(0.5f);
  CHECK_EQ(world.sprite().textureRect.x, 10 + 2 * (32 + 2));
  CHECK_EQ(world.sprite().textureRect.y, 20);
  CHECK_EQ(world.sprite().textureRect.width, 32);
  CHECK_EQ(world.sprite().textureRect.height, 16);
}

TEST(AnimationSystem, ColumnsWrapToTheNextRow) {
  auto animation = strip(6);
  animation.columns = 2;
  animation.paddingY = 4;
  AnimationWorld world(animation);

  world.run(0.75f);
  // frame 3 is column 1 of row 1
  CHECK_EQ(world.sprite().textureRect.x, 32);
  CHECK_EQ(world.sprite().textureRect.y, 16 + 4);
}

TEST(AnimationSystem, ReversePlaysFromTheEnd) {
  auto animation = strip(4);
  animation.reverse = true;
  AnimationWorld world(animation);

  world.run(0.01f);
  CHECK_EQ(world.sprite().textureRect.x, 3 * 32);
  world.run(0.25f);
  CHECK_EQ(world.sprite().textureRect.x, 2 * 32);
}

TEST(AnimationSystem, ZeroSizeKeepsTheSpriteRect) {
  auto animation = strip(2);
  animation.frameWidth = 0;
  animation.frameHeight = 0;
  AnimationWorld world(animation);
  world.sprite().textureRect.width = 77;
  world.sprite().textureRect.height = 66;

  world.run(0.01f);
  CHECK_EQ(world.sprite().textureRect.width, 77);
  CHECK_EQ(world.sprite().textureRect.height, 66);
}

// AnimatorSystem

namespace {

struct AnimatorWorld {
  Registry registry;
  std::size_t entity;
  rtk::systems::AnimatorSystem system;

  AnimatorWorld() {
    registry.register_component<component::animator>();
    registry.register_component<component::Animation>();
    entity = registry.spawn_entity();
    component::animator anim;
    auto &up = anim.clips[static_cast<std::size_t>(component::animState::UP)];
    up.startX = 64;
    up.startY = 8;
    up.totalFrames = 3;
    up.frameDuration = 0.5f;
    up.loop = false;
    up.frameWidth = 40;
    registry.get_components<component::animator>().insert_at(entity, anim);
    registry.get_components<component::Animation>().insert_at(entity, strip(5));
  }

  component::animator &animator() {
    return *registry.get_components<component::animator>().get(entity);
  }
  component::Animation &animation() {
    return *registry.get_components<component::Animation>().get(entity);
  }
  void run() { system.update(registry, 0.016f); }
};

} // namespace

TEST(AnimatorSystem, LoadsTheClipOfTheState) {
  AnimatorWorld world;
  world.animator().currentState = component::animState::UP;

  world.run();
  CHECK_EQ(world.animation().startX, 64);
  CHECK_EQ(world.animation().startY, 8);
  CHECK_EQ(world.animation().totalFrames, 3);
  CHECK_EQ(world.animation().frameDuration, 0.5f);
  CHECK_FALSE(world.animation().loop);
  CHECK_EQ(world.animator().previousState, component::animState::UP);
}

TEST(AnimatorSystem, RestartsTheAnimation) {
  AnimatorWorld world;
  world.animation().currentFrame = 4;
  world.animation().elapsedTime = 0.2f;
  world.animation().playing = false;
  world.animator().currentState = component::animState::UP;

  world.run();
  CHECK_EQ(world.animation().currentFrame, 0);
  CHECK_EQ(world.animation().elapsedTime, 0.0f);
  CHECK(world.animation().playing);
}

TEST(AnimatorSystem, SameStateDoesNotReload) {
  AnimatorWorld world;
  world.animator().currentState = component::animState::UP;
  world.run();
  world.animation().currentFrame = 2;

  world.run();
  CHECK_EQ(world.animation().currentFrame, 2);
}

TEST(AnimatorSystem, ZeroClipSizeKeepsTheFrameSize) {
  AnimatorWorld world;
  world.animator().currentState = component::animState::UP;

  world.run();
  CHECK_EQ(world.animation().frameWidth, 40);
  CHECK_EQ(world.animation().frameHeight, 16);
}

TEST(AnimatorSystem, FirstUpdateLoadsTheIdleClip) {
  AnimatorWorld world;
  world.animator().clips[0].startX = 5;

  world.run();
  CHECK_EQ(world.animation().startX, 5);
  CHECK_EQ(world.animator().previousState, component::animState::IDLE);
}

// AnimationCleanSystem

TEST(AnimationCleanSystem, FinishedAnimationMarksDead) {
  Registry registry;
  registry.register_component<component::DestroyAnimEnd>();
  registry.register_component<component::Animation>();
  registry.register_component<component::Dead>();
  const auto entity = registry.spawn_entity();
  auto animation = strip(3);
  animation.playing = false;
  registry.get_components<component::Animation>().insert_at(entity, animation);
  registry.get_components<component::DestroyAnimEnd>().insert_at(entity, {});

  rtk::systems::AnimationCleanSystem system;
  system.update(registry, 0.016f);
  CHECK(registry.has_component<component::Dead>(entity));
}

TEST(AnimationCleanSystem, PlayingAnimationIsKept) {
  Registry registry;
  registry.register_component<component::DestroyAnimEnd>();
  registry.register_component<component::Animation>();
  registry.register_component<component::Dead>();
  const auto entity = registry.spawn_entity();
  registry.get_components<component::Animation>().insert_at(entity, strip(3));
  registry.get_components<component::DestroyAnimEnd>().insert_at(entity, {});

  rtk::systems::AnimationCleanSystem system;
  system.update(registry, 0.016f);
  CHECK_FALSE(registry.has_component<component::Dead>(entity));
}

TEST(AnimationCleanSystem, NeedsTheDestroyTag) {
  Registry registry;
  registry.register_component<component::DestroyAnimEnd>();
  registry.register_component<component::Animation>();
  registry.register_component<component::Dead>();
  const auto entity = registry.spawn_entity();
  auto animation = strip(3);
  animation.playing = false;
  registry.get_components<component::Animation>().insert_at(entity, animation);

  rtk::systems::AnimationCleanSystem system;
  system.update(registry, 0.016f);
  CHECK_FALSE(registry.has_component<component::Dead>(entity));
}

// ParallaxSystem

namespace {

struct ParallaxWorld {
  Registry registry;
  rtk::systems::ParallaxSystem system;

  ParallaxWorld() {
    registry.register_component<Transform>();
    registry.register_component<component::Parallax>();
    registry.register_component<AABBCollider>();
    registry.register_component<Velocity>();
  }

  std::size_t layer(float x, float speed, float loopWidth) {
    const auto entity = registry.spawn_entity();
    registry.get_components<Transform>().insert_at(entity, at(x, 0));
    registry.get_components<component::Parallax>().insert_at(
        entity, component::Parallax{speed, loopWidth});
    return entity;
  }

  float x(std::size_t entity) {
    return registry.get_components<Transform>().get(entity)->position.x;
  }
};

} // namespace

TEST(ParallaxSystem, ScrollsLeft) {
  ParallaxWorld world;
  const auto layer = world.layer(100, 50, 1920);

  world.system.update(world.registry, 0.5f);
  CHECK_EQ(world.x(layer), 75.0f);
}

TEST(ParallaxSystem, WrapsAfterALoopWidth) {
  ParallaxWorld world;
  const auto layer = world.layer(-90, 20, 100);

  world.system.update(world.registry, 0.5f);
  CHECK_EQ(world.x(layer), 200.0f);
}

TEST(ParallaxSystem, DoesNotWrapBeforeTheLoopWidth) {
  ParallaxWorld world;
  const auto layer = world.layer(-80, 20, 100);

  world.system.update(world.registry, 0.5f);
  CHECK_EQ(world.x(layer), -90.0f);
}

TEST(ParallaxSystem, BossRoomStopsTheScroll) {
  ParallaxWorld world;
  const auto layer = world.layer(100, 50, 1920);
  const auto room = world.registry.spawn_entity();
  world.registry.get_components<Transform>().insert_at(room, at(-3, 0));
  world.registry.get_components<AABBCollider>().insert_at(
      room, box(100, 100, Collider::BossRoom));

  world.system.update(world.registry, 1.0f);
  CHECK_EQ(world.x(layer), 100.0f);
  CHECK_EQ(world.x(room), 0.0f);
  CHECK_EQ(
      world.registry.get_components<component::Parallax>().get(layer)->speedX,
      0.0f);
}

TEST(ParallaxSystem, BossRoomStopsTheStage) {
  ParallaxWorld world;
  const auto room = world.registry.spawn_entity();
  world.registry.get_components<Transform>().insert_at(room, at(0, 0));
  world.registry.get_components<AABBCollider>().insert_at(
      room, box(100, 100, Collider::BossRoom));
  world.registry.get_components<Velocity>().insert_at(room, Velocity{-30, 0});
  const auto wall = world.registry.spawn_entity();
  world.registry.get_components<AABBCollider>().insert_at(
      wall, box(10, 10, Collider::Environment));
  world.registry.get_components<Velocity>().insert_at(wall, Velocity{-30, 2});
  const auto enemy = world.registry.spawn_entity();
  world.registry.get_components<AABBCollider>().insert_at(
      enemy, box(10, 10, Collider::Enemy));
  world.registry.get_components<Velocity>().insert_at(enemy, Velocity{-30, 0});

  world.system.update(world.registry, 0.016f);
  auto &velocities = world.registry.get_components<Velocity>();
  CHECK_EQ(velocities.get(room)->x, 0.0f);
  CHECK_EQ(velocities.get(wall)->x, 0.0f);
  CHECK_EQ(velocities.get(wall)->y, 2.0f);
  CHECK_EQ(velocities.get(enemy)->x, -30.0f);
}

TEST(ParallaxSystem, BossRoomAheadDoesNotStop) {
  ParallaxWorld world;
  const auto layer = world.layer(100, 50, 1920);
  const auto room = world.registry.spawn_entity();
  world.registry.get_components<Transform>().insert_at(room, at(500, 0));
  world.registry.get_components<AABBCollider>().insert_at(
      room, box(100, 100, Collider::BossRoom));

  world.system.update(world.registry, 1.0f);
  CHECK_EQ(world.x(layer), 50.0f);
  CHECK_EQ(world.x(room), 500.0f);
}

// AudioSystem

namespace {

struct FakeAudio {
  int updates = 0;
  std::size_t addSound(const std::string &) { return 1; }
  void playSound(std::size_t) {}
  void removeSound(std::size_t) {}
  void update() { ++updates; }
};

static_assert(rtk::ecs::concepts::IsAudioManager<FakeAudio>);

} // namespace

TEST(AudioSystem, UpdatesTheManagerEveryFrame) {
  Registry registry;
  FakeAudio audio;
  rtk::systems::AudioSystem<FakeAudio> system(audio);

  system.update(registry, 0.016f);
  system.update(registry, 0.016f);
  system.update(registry, 0.016f);
  CHECK_EQ(audio.updates, 3);
}

TEST(AudioSystem, StartAndStopDoNotUpdate) {
  Registry registry;
  FakeAudio audio;
  rtk::systems::AudioSystem<FakeAudio> system(audio);

  system.onStart(registry);
  system.onStop(registry);
  CHECK_EQ(audio.updates, 0);
}

// LuaScriptSystem

namespace {

/// Lua script written to a temporary file, removed at the end of the test.
struct TempScript {
  std::filesystem::path path;

  TempScript(const std::string &name, const std::string &code) {
    path = std::filesystem::temp_directory_path() /
           ("rtype_test_" + name + ".lua");
    std::ofstream(path) << code;
  }
  ~TempScript() {
    std::error_code error;
    std::filesystem::remove(path, error);
  }
};

struct LuaWorld {
  Registry registry;
  std::size_t entity;
  rtk::systems::LuaScriptSystem system;

  LuaWorld(const std::string &script, Velocity velocity) {
    registry.register_component<component::LuaScript>();
    registry.register_component<Velocity>();
    entity = registry.spawn_entity();
    registry.get_components<component::LuaScript>().insert_at(
        entity, component::LuaScript{script, 0.0f});
    registry.get_components<Velocity>().insert_at(entity, velocity);
  }

  Velocity &velocity() {
    return *registry.get_components<Velocity>().get(entity);
  }
  component::LuaScript &script() {
    return *registry.get_components<component::LuaScript>().get(entity);
  }
};

} // namespace

TEST(LuaScriptSystem, ScriptSetsTheVelocity) {
  TempScript script("velocity", R"(
function update_velocity(time, dt, vx, vy)
  return vx + 10, time * 2
end
)");
  LuaWorld world(script.path.string(), Velocity{5, 0});

  world.system.onStart(world.registry);
  world.system.update(world.registry, 0.5f);
  CHECK_EQ(world.velocity().x, 15.0f);
  CHECK_EQ(world.velocity().y, 1.0f);
  world.system.onStop(world.registry);
}

TEST(LuaScriptSystem, TimeAccumulates) {
  TempScript script("time", R"(
function update_velocity(time, dt, vx, vy)
  return time, dt
end
)");
  LuaWorld world(script.path.string(), Velocity{});

  world.system.onStart(world.registry);
  world.system.update(world.registry, 0.25f);
  world.system.update(world.registry, 0.5f);
  CHECK_EQ(world.script().time, 0.75f);
  CHECK_EQ(world.velocity().x, 0.75f);
  CHECK_EQ(world.velocity().y, 0.5f);
}

TEST(LuaScriptSystem, MissingScriptKeepsTheVelocity) {
  LuaWorld world("/nonexistent/rtype_missing.lua", Velocity{3, 4});

  world.system.onStart(world.registry);
  world.system.update(world.registry, 0.5f);
  CHECK_EQ(world.velocity().x, 3.0f);
  CHECK_EQ(world.velocity().y, 4.0f);
}

TEST(LuaScriptSystem, ScriptWithoutFunctionKeepsTheVelocity) {
  TempScript script("nofunction", "local x = 1\n");
  LuaWorld world(script.path.string(), Velocity{3, 4});

  world.system.onStart(world.registry);
  world.system.update(world.registry, 0.5f);
  CHECK_EQ(world.velocity().x, 3.0f);
}

TEST(LuaScriptSystem, NonNumericResultIsIgnored) {
  TempScript script("strings", R"(
function update_velocity(time, dt, vx, vy)
  return "a", "b"
end
)");
  LuaWorld world(script.path.string(), Velocity{3, 4});

  world.system.onStart(world.registry);
  world.system.update(world.registry, 0.5f);
  CHECK_EQ(world.velocity().x, 3.0f);
  CHECK_EQ(world.velocity().y, 4.0f);
}

TEST(LuaScriptSystem, RuntimeErrorIsCaught) {
  TempScript script("error", R"(
function update_velocity(time, dt, vx, vy)
  error("boom")
end
)");
  LuaWorld world(script.path.string(), Velocity{3, 4});

  world.system.onStart(world.registry);
  CHECK_NOTHROW(world.system.update(world.registry, 0.5f));
  CHECK_EQ(world.velocity().x, 3.0f);
}

TEST(LuaScriptSystem, DoesNothingBeforeStart) {
  TempScript script("nostart", R"(
function update_velocity(time, dt, vx, vy)
  return 100, 100
end
)");
  LuaWorld world(script.path.string(), Velocity{1, 1});

  world.system.update(world.registry, 0.5f);
  CHECK_EQ(world.velocity().x, 1.0f);
  CHECK_EQ(world.script().time, 0.0f);
}

TEST(LuaScriptSystem, EmptyPathIsSkipped) {
  LuaWorld world("", Velocity{1, 2});

  world.system.onStart(world.registry);
  world.system.update(world.registry, 0.5f);
  CHECK_EQ(world.velocity().y, 2.0f);
  CHECK_EQ(world.script().time, 0.0f);
}
