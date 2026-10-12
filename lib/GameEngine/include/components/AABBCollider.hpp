#pragma once

#include <cstdint>
#include <string>
#include <nlohmann/json.hpp>
#include <Registry.hpp>

namespace Collider {
enum CollisionLayer : std::uint32_t {
  Player = 1 << 0,
  Enemy = 1 << 1,
  PlayerProjectile = 1 << 2,
  EnemyProjectile = 1 << 3,
  Environment = 1 << 4
};
}

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

inline std::uint32_t stringToLayer(const std::string& name) {
    if (name == "Player") return Collider::CollisionLayer::Player;
    if (name == "Enemy") return Collider::CollisionLayer::Enemy;
    if (name == "PlayerProjectile") return Collider::CollisionLayer::PlayerProjectile;
    if (name == "EnemyProjectile") return Collider::CollisionLayer::EnemyProjectile;
    if (name == "Environment") return Collider::CollisionLayer::Environment;
    return 0;
}

inline void aabbColliderFromJson(rtk::ecs::Registry& reg, std::size_t entity, const nlohmann::json& data) {
    component::AABBCollider collider{};
    collider.offsetX = data.value("offsetX", data.value("offset_x", 0.0f));
    collider.offsetY = data.value("offsetY", data.value("offset_y", 0.0f));
    collider.width = data.value("width", 0.0f);
    collider.height = data.value("height", 0.0f);
    collider.trigger = data.value("trigger", true);

    if (data.contains("layer") && data["layer"].is_string()) {
        collider.layer = stringToLayer(data["layer"].get<std::string>());
    } else {
        collider.layer = data.value("layer", 1u);
    }

    if (data.contains("mask") && data["mask"].is_array()) {
        collider.mask = 0;
        for (const auto& maskStr : data["mask"]) {
            collider.mask |= stringToLayer(maskStr.get<std::string>());
        }
    } else {
        collider.mask = data.value("mask", 0xFFFFFFFFu);
    }

    reg.get_components<component::AABBCollider>().insert_at(entity, collider);
}

} // namespace component