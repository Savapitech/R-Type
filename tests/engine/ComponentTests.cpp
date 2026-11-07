#include "Test.hpp"

#include <Registry.hpp>
#include <nlohmann/json.hpp>

#include "components/AABBCollider.hpp"
#include "components/Animation.hpp"
#include "components/Animator.hpp"
#include "components/AttachedTo.hpp"
#include "components/Dead.hpp"
#include "components/DestroyAnimEnd.hpp"
#include "components/LuaScript.hpp"
#include "components/NetworkEntity.hpp"
#include "components/Parallax.hpp"
#include "components/SoundRequest.hpp"
#include "components/Transform.hpp"
#include "components/Velocity.hpp"

using json = nlohmann::json;

namespace {

/// Registry with one entity and the component T registered.
template <typename T> struct Fixture {
  rtk::ecs::Registry registry;
  std::size_t entity;

  Fixture() {
    registry.register_component<T>();
    entity = registry.spawn_entity();
  }

  T *get() { return registry.get_components<T>().get(entity); }
};

} // namespace

// Transform

TEST(TransformJson, ReadsNestedPositionAndScale) {
  Fixture<component::Transform> fixture;

  component::transformFromJson(
      fixture.registry, fixture.entity,
      json::parse(R"({"position": {"x": 10, "y": -4}, "rotation": 1.5,
                      "scale": {"x": 2, "y": 3}})"));
  const auto *transform = fixture.get();
  CHECK(transform != nullptr);
  CHECK_EQ(transform->position.x, 10.0f);
  CHECK_EQ(transform->position.y, -4.0f);
  CHECK_EQ(transform->rotation, 1.5f);
  CHECK_EQ(transform->scale.x, 2.0f);
  CHECK_EQ(transform->scale.y, 3.0f);
}

TEST(TransformJson, ReadsFlatPosition) {
  Fixture<component::Transform> fixture;

  component::transformFromJson(fixture.registry, fixture.entity,
                               json::parse(R"({"x": 5, "y": 6})"));
  CHECK_EQ(fixture.get()->position.x, 5.0f);
  CHECK_EQ(fixture.get()->position.y, 6.0f);
}

TEST(TransformJson, DefaultsToOriginAndUnitScale) {
  Fixture<component::Transform> fixture;

  component::transformFromJson(fixture.registry, fixture.entity,
                               json::object());
  const auto *transform = fixture.get();
  CHECK_EQ(transform->position.x, 0.0f);
  CHECK_EQ(transform->position.y, 0.0f);
  CHECK_EQ(transform->rotation, 0.0f);
  CHECK_EQ(transform->scale.x, 1.0f);
  CHECK_EQ(transform->scale.y, 1.0f);
}

TEST(TransformJson, MissingScaleAxisDefaultsToOne) {
  Fixture<component::Transform> fixture;

  component::transformFromJson(fixture.registry, fixture.entity,
                               json::parse(R"({"scale": {"x": 4}})"));
  CHECK_EQ(fixture.get()->scale.x, 4.0f);
  CHECK_EQ(fixture.get()->scale.y, 1.0f);
}

TEST(TransformJson, ReplacesAnExistingTransform) {
  Fixture<component::Transform> fixture;

  component::transformFromJson(fixture.registry, fixture.entity,
                               json::parse(R"({"x": 1})"));
  component::transformFromJson(fixture.registry, fixture.entity,
                               json::parse(R"({"x": 2})"));
  CHECK_EQ(fixture.registry.get_components<component::Transform>().size(), 1u);
  CHECK_EQ(fixture.get()->position.x, 2.0f);
}

// Velocity

TEST(VelocityJson, ReadsBothAxes) {
  Fixture<component::Velocity> fixture;

  component::velocityFromJson(fixture.registry, fixture.entity,
                              json::parse(R"({"x": -120.5, "y": 30})"));
  CHECK_EQ(fixture.get()->x, -120.5f);
  CHECK_EQ(fixture.get()->y, 30.0f);
}

TEST(VelocityJson, DefaultsToZero) {
  Fixture<component::Velocity> fixture;

  component::velocityFromJson(fixture.registry, fixture.entity, json::object());
  CHECK_EQ(fixture.get()->x, 0.0f);
  CHECK_EQ(fixture.get()->y, 0.0f);
}

// AABBCollider

TEST(AABBColliderJson, StringToLayerKnowsEveryLayer) {
  CHECK_EQ(component::stringToLayer("Player"), Collider::Player);
  CHECK_EQ(component::stringToLayer("Enemy"), Collider::Enemy);
  CHECK_EQ(component::stringToLayer("PlayerProjectile"),
           Collider::PlayerProjectile);
  CHECK_EQ(component::stringToLayer("EnemyProjectile"),
           Collider::EnemyProjectile);
  CHECK_EQ(component::stringToLayer("Environment"), Collider::Environment);
  CHECK_EQ(component::stringToLayer("PowerUp"), Collider::PowerUp);
  CHECK_EQ(component::stringToLayer("BossRoom"), Collider::BossRoom);
}

TEST(AABBColliderJson, UnknownLayerIsZero) {
  CHECK_EQ(component::stringToLayer("Nothing"), 0u);
  CHECK_EQ(component::stringToLayer(""), 0u);
  CHECK_EQ(component::stringToLayer("player"), 0u);
}

TEST(AABBColliderJson, LayersAreDistinctBits) {
  const std::uint32_t layers[] = {
      Collider::Player,           Collider::Enemy,
      Collider::PlayerProjectile, Collider::EnemyProjectile,
      Collider::Environment,      Collider::PowerUp,
      Collider::BossRoom};
  std::uint32_t seen = 0;
  for (auto layer : layers) {
    CHECK_EQ(layer & (layer - 1), 0u);
    CHECK_EQ(seen & layer, 0u);
    seen |= layer;
  }
}

TEST(AABBColliderJson, ReadsNamedLayerAndMask) {
  Fixture<component::AABBCollider> fixture;

  component::aabbColliderFromJson(
      fixture.registry, fixture.entity,
      json::parse(R"({"width": 32, "height": 16, "offsetX": 2, "offsetY": 3,
                      "layer": "Enemy", "mask": ["Player", "PlayerProjectile"],
                      "trigger": false})"));
  const auto *collider = fixture.get();
  CHECK_EQ(collider->width, 32.0f);
  CHECK_EQ(collider->height, 16.0f);
  CHECK_EQ(collider->offsetX, 2.0f);
  CHECK_EQ(collider->offsetY, 3.0f);
  CHECK_EQ(collider->layer, static_cast<std::uint32_t>(Collider::Enemy));
  CHECK_EQ(collider->mask, static_cast<std::uint32_t>(
                               Collider::Player | Collider::PlayerProjectile));
  CHECK_FALSE(collider->trigger);
}

TEST(AABBColliderJson, AcceptsSnakeCaseOffsets) {
  Fixture<component::AABBCollider> fixture;

  component::aabbColliderFromJson(
      fixture.registry, fixture.entity,
      json::parse(R"({"offset_x": 7, "offset_y": 8})"));
  CHECK_EQ(fixture.get()->offsetX, 7.0f);
  CHECK_EQ(fixture.get()->offsetY, 8.0f);
}

TEST(AABBColliderJson, AcceptsNumericLayerAndMask) {
  Fixture<component::AABBCollider> fixture;

  component::aabbColliderFromJson(fixture.registry, fixture.entity,
                                  json::parse(R"({"layer": 4, "mask": 3})"));
  CHECK_EQ(fixture.get()->layer, 4u);
  CHECK_EQ(fixture.get()->mask, 3u);
}

TEST(AABBColliderJson, DefaultsCollideWithEverything) {
  Fixture<component::AABBCollider> fixture;

  component::aabbColliderFromJson(fixture.registry, fixture.entity,
                                  json::object());
  CHECK_EQ(fixture.get()->layer, 1u);
  CHECK_EQ(fixture.get()->mask, 0xFFFFFFFFu);
  CHECK(fixture.get()->trigger);
  CHECK_EQ(fixture.get()->width, 0.0f);
}

TEST(AABBColliderJson, EmptyMaskArrayMatchesNothing) {
  Fixture<component::AABBCollider> fixture;

  component::aabbColliderFromJson(fixture.registry, fixture.entity,
                                  json::parse(R"({"mask": []})"));
  CHECK_EQ(fixture.get()->mask, 0u);
}

// Tags and small components

TEST(TagJson, DeadIsAdded) {
  Fixture<component::Dead> fixture;

  component::deadFromJson(fixture.registry, fixture.entity, json::object());
  CHECK(fixture.registry.has_component<component::Dead>(fixture.entity));
}

TEST(TagJson, DestroyAnimEndIsAdded) {
  Fixture<component::DestroyAnimEnd> fixture;

  component::destroyAnimEndFromJson(fixture.registry, fixture.entity,
                                    json::object());
  CHECK(fixture.registry.has_component<component::DestroyAnimEnd>(
      fixture.entity));
}

TEST(AttachedToJson, ReadsTargetAndOffsets) {
  Fixture<component::AttachedTo> fixture;

  component::attachedToFromJson(
      fixture.registry, fixture.entity,
      json::parse(R"({"targetEntity": 3, "offsetX": 10, "offsetY": -2})"));
  CHECK_EQ(fixture.get()->targetEntity, 3u);
  CHECK_EQ(fixture.get()->offsetX, 10.0f);
  CHECK_EQ(fixture.get()->offsetY, -2.0f);
}

TEST(NetworkEntityJson, ReadsIdAndArchetype) {
  Fixture<component::NetworkEntity> fixture;

  component::networkEntityFromJson(
      fixture.registry, fixture.entity,
      json::parse(R"({"id": 1001, "archetypeId": 65535})"));
  CHECK_EQ(fixture.get()->id, 1001u);
  CHECK_EQ(fixture.get()->archetypeId, 65535);
}

TEST(ParallaxJson, ReadsValues) {
  Fixture<component::Parallax> fixture;

  component::parallaxFromJson(
      fixture.registry, fixture.entity,
      json::parse(R"({"speedX": 12.5, "loopWidth": 800})"));
  CHECK_EQ(fixture.get()->speedX, 12.5f);
  CHECK_EQ(fixture.get()->loopWidth, 800.0f);
}

TEST(ParallaxJson, HasScreenSizedDefaults) {
  Fixture<component::Parallax> fixture;

  component::parallaxFromJson(fixture.registry, fixture.entity, json::object());
  CHECK_EQ(fixture.get()->speedX, 50.0f);
  CHECK_EQ(fixture.get()->loopWidth, 1920.0f);
}

TEST(LuaScriptJson, ReadsScriptPath) {
  Fixture<component::LuaScript> fixture;

  component::luaScriptFromJson(
      fixture.registry, fixture.entity,
      json::parse(R"({"script": "assets/scripts/wave.lua", "time": 2})"));
  CHECK_EQ(fixture.get()->scriptPath, std::string("assets/scripts/wave.lua"));
  CHECK_EQ(fixture.get()->time, 2.0f);
}

TEST(LuaScriptJson, DefaultsToNoScript) {
  Fixture<component::LuaScript> fixture;

  component::luaScriptFromJson(fixture.registry, fixture.entity,
                               json::object());
  CHECK(fixture.get()->scriptPath.empty());
  CHECK_EQ(fixture.get()->time, 0.0f);
}

TEST(SoundRequestJson, ReadsSoundId) {
  Fixture<component::SoundRequest> fixture;

  component::soundRequestFromJson(fixture.registry, fixture.entity,
                                  json::parse(R"({"soundId": 4})"));
  CHECK_EQ(fixture.get()->soundId, 4u);
}

// Animation

TEST(AnimationJson, ReadsEveryField) {
  Fixture<component::Animation> fixture;

  component::animationFromJson(
      fixture.registry, fixture.entity,
      json::parse(R"({"frameDuration": 0.05, "totalFrames": 8,
                      "frameWidth": 33, "frameHeight": 17, "startX": 1,
                      "startY": 2, "columns": 4, "paddingX": 1,
                      "paddingY": 2, "loop": false, "playing": false,
                      "reverse": true, "currentFrame": 3})"));
  const auto *animation = fixture.get();
  CHECK_NEAR(animation->frameDuration, 0.05, 1e-6);
  CHECK_EQ(animation->totalFrames, 8);
  CHECK_EQ(animation->frameWidth, 33);
  CHECK_EQ(animation->frameHeight, 17);
  CHECK_EQ(animation->startX, 1);
  CHECK_EQ(animation->startY, 2);
  CHECK_EQ(animation->columns, 4);
  CHECK_EQ(animation->paddingX, 1);
  CHECK_EQ(animation->paddingY, 2);
  CHECK_FALSE(animation->loop);
  CHECK_FALSE(animation->playing);
  CHECK(animation->reverse);
  CHECK_EQ(animation->currentFrame, 3);
}

TEST(AnimationJson, DefaultsToASinglePlayingFrame) {
  Fixture<component::Animation> fixture;

  component::animationFromJson(fixture.registry, fixture.entity,
                               json::object());
  const auto *animation = fixture.get();
  CHECK_EQ(animation->totalFrames, 1);
  CHECK_EQ(animation->currentFrame, 0);
  CHECK(animation->loop);
  CHECK(animation->playing);
  CHECK_FALSE(animation->reverse);
}

// Animator

TEST(AnimatorJson, StringToAnimState) {
  CHECK_EQ(component::stringToAnimState("UP"), component::animState::UP);
  CHECK_EQ(component::stringToAnimState("DOWN"), component::animState::DOWN);
  CHECK_EQ(component::stringToAnimState("DIE"), component::animState::DIE);
  CHECK_EQ(component::stringToAnimState("SHOOT"), component::animState::SHOOT);
  CHECK_EQ(component::stringToAnimState("IDLE"), component::animState::IDLE);
  CHECK_EQ(component::stringToAnimState("whatever"),
           component::animState::IDLE);
}

TEST(AnimatorJson, ReadsClipsByStateName) {
  Fixture<component::animator> fixture;

  component::animatorFromJson(fixture.registry, fixture.entity,
                              json::parse(R"({"currentState": "UP", "clips": {
                        "UP": {"startX": 66, "totalFrames": 2, "loop": false},
                        "DIE": {"startY": 40, "frameWidth": 64}}})"));
  const auto *anim = fixture.get();
  const auto &up =
      anim->clips[static_cast<std::size_t>(component::animState::UP)];
  const auto &die =
      anim->clips[static_cast<std::size_t>(component::animState::DIE)];
  CHECK_EQ(anim->currentState, component::animState::UP);
  CHECK_EQ(anim->previousState, component::animState::NONE);
  CHECK_EQ(up.startX, 66);
  CHECK_EQ(up.totalFrames, 2);
  CHECK_FALSE(up.loop);
  CHECK_EQ(die.startY, 40);
  CHECK_EQ(die.frameWidth, 64);
}

TEST(AnimatorJson, AcceptsNumericState) {
  Fixture<component::animator> fixture;

  component::animatorFromJson(fixture.registry, fixture.entity,
                              json::parse(R"({"currentState": 2})"));
  CHECK_EQ(fixture.get()->currentState, component::animState::DOWN);
}

TEST(AnimatorJson, DefaultsToIdle) {
  Fixture<component::animator> fixture;

  component::animatorFromJson(fixture.registry, fixture.entity, json::object());
  CHECK_EQ(fixture.get()->currentState, component::animState::IDLE);
  CHECK_EQ(fixture.get()->clips.size(),
           static_cast<std::size_t>(component::animState::COUNT));
}
