#pragma once

#include <Registry.hpp>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>

/**
 * @brief Collision layer definitions.
 */
namespace Collider {
/**
 * @addtogroup api_engine
 * @{
 */

/**
 * @brief Bit flags naming the group a collider belongs to.
 */
enum CollisionLayer : std::uint32_t {
  Player = 1 << 0,           ///< Player ships.
  Enemy = 1 << 1,            ///< Bydos enemies.
  PlayerProjectile = 1 << 2, ///< Missiles fired by players.
  EnemyProjectile = 1 << 3,  ///< Missiles fired by enemies.
  Environment = 1 << 4,      ///< Stage walls and obstacles.
  PowerUp = 1 << 5           ///< PowerUp.
};

/** @} */
} // namespace Collider

namespace component {
/**
 * @addtogroup api_engine
 * @{
 */

/**
 * @brief Axis aligned bounding box used for collision detection.
 */
struct AABBCollider {
  float offsetX = 0.f; ///< Box offset from the entity position on x.
  float offsetY = 0.f; ///< Box offset from the entity position on y.
  float width = 0.f;   ///< Box width before scaling.
  float height = 0.f;  ///< Box height before scaling.

  std::uint32_t layer = 1;         ///< Layers this collider belongs to.
  std::uint32_t mask = 0xFFFFFFFF; ///< Layers this collider reacts to.

  bool trigger = true; ///< Reports overlaps without physical response.
};

/**
 * @brief Converts a layer name to its bit flag.
 * @param name Layer name, such as `Player` or `Environment`.
 * @return The layer flag, or 0 if the name is unknown.
 */
inline std::uint32_t stringToLayer(const std::string &name) {
  if (name == "Player")
    return Collider::CollisionLayer::Player;
  if (name == "Enemy")
    return Collider::CollisionLayer::Enemy;
  if (name == "PlayerProjectile")
    return Collider::CollisionLayer::PlayerProjectile;
  if (name == "EnemyProjectile")
    return Collider::CollisionLayer::EnemyProjectile;
  if (name == "Environment")
    return Collider::CollisionLayer::Environment;
  return 0;
}

/**
 * @brief Adds an AABBCollider to an entity from its blueprint data.
 * @param reg Registry owning the entity.
 * @param entity Target entity.
 * @param data JSON object with size, offset, layer name and mask names.
 */
inline void aabbColliderFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                                 const nlohmann::json &data) {
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
    for (const auto &maskStr : data["mask"]) {
      collider.mask |= stringToLayer(maskStr.get<std::string>());
    }
  } else {
    collider.mask = data.value("mask", 0xFFFFFFFFu);
  }

  reg.get_components<component::AABBCollider>().insert_at(entity, collider);
}

/** @} */
} // namespace component
