#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>

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
  float damage{10.0f}; ///< Damage dealt on hit.
};

/**
 * @brief Adds a Bullet to an entity from its blueprint data.
 * @param reg Registry owning the entity.
 * @param entity Target entity.
 * @param data JSON object with an optional `damage` field.
 */
inline void bulletFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                           const nlohmann::json &data) {
  component::Bullet bullet{};
  bullet.damage = data.value("damage", 10.0f);

  reg.get_components<component::Bullet>().insert_at(entity, bullet);
}

/** @} */
} // namespace component
