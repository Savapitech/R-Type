#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>

#include <Registry.hpp>

namespace component {
struct AABBCollider {
  float offsetX = 0.f;
  float offsetY = 0.f;
  float width = 0.f;
  float height = 0.f;

  std::uint32_t layer = 1;
  std::uint32_t mask = 0xFFFFFFFF;

  bool trigger = true;
};

inline void aabbColliderFromJson(rtk::ecs::Registry& reg, std::size_t entity, const nlohmann::json& data) {
    component::AABBCollider collider{};
    collider.offsetX = data.value("offsetX", data.value("offset_x", 0.0f));
    collider.offsetY = data.value("offsetY", data.value("offset_y", 0.0f));
    collider.width = data.value("width", 0.0f);
    collider.height = data.value("height", 0.0f);
    collider.layer = data.value("layer", 1u);
    collider.mask = data.value("mask", 0xFFFFFFFFu);
    collider.trigger = data.value("trigger", true);

    reg.get_components<component::AABBCollider>().insert_at(entity, collider);
}

} // namespace component

namespace Collider {
enum CollisionLayer : std::uint32_t {
  Player = 1 << 0,
  Enemy = 1 << 1,
  PlayerProjectile = 1 << 2,
  EnemyProjectile = 1 << 3,
  Environment = 1 << 4
};
}
