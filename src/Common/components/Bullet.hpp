#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <vector>

#include <Registry.hpp>

namespace component {
/**
 * @addtogroup api_gameplay
 * @{
 */

/**
 * @brief Marks an entity as a projectile.
 */
struct Bullet {
  float damage{10.0f};                    ///< Damage dealt on hit.
  bool piercing{false};                   ///< True if projectile passes through enemies.
  std::vector<std::size_t> hitEntities{}; ///< Enemies already damaged by this projectile.
};

/**
 * @brief Adds a Bullet to an entity from its blueprint data.
 * @param reg Registry owning the entity.
 * @param entity Target entity.
 * @param data JSON object with optional `damage` and `piercing` fields.
 */
inline void bulletFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                           const nlohmann::json &data) {
  component::Bullet bullet{};
  bullet.damage = data.value("damage", 10.0f);
  bullet.piercing = data.value("piercing", false);

  reg.get_components<component::Bullet>().insert_at(entity, bullet);
}

/** @} */
} // namespace component
