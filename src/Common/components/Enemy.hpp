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
 * @brief Marks an entity as a Bydos enemy.
 */
struct Enemy {
  float health{100.0f};  ///< Remaining health points.
  float fireTimer{0.0f}; ///< Timer to open fire
  float fireRate{2.0f};  ///< Fire rate in second
  std::string blueprintName{
      "enemy_bullet"};   ///< Blueprint of the enemy's bullet
  float offsetX{-20.0f}; ///< OffsetX for the spawn of the bullet.
  float offsetY{16.0f};  ///< OffsetY for the spawn of the bullet.
  int burstNumber{1};    ///< Number of bullet per burst.
  float burstInterval{
      0.1f};              ///< Interval between the bullets of the same burst.
  int burstRemaining{0};  ///< Number of rest of bullets in the burst.
  float burstTimer{0.0f}; ///< Timer to open the burst.
};

/**
 * @brief Adds an Enemy to an entity from its blueprint data.
 * @param reg Registry owning the entity.
 * @param entity Target entity.
 * @param data JSON object with an optional `health` field.
 */
inline void enemyFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                          const nlohmann::json &data) {
  component::Enemy enemy{};
  enemy.health = data.value("health", 100.0f);
  enemy.fireRate = data.value("fireRate", 2.0f);
  enemy.fireTimer = data.value("fireDelay", enemy.fireRate);
  enemy.blueprintName = data.value("blueprintName", "enemy_bullet");
  enemy.offsetX = data.value("offsetX", -20.0f);
  enemy.offsetY = data.value("offsetY", 16.0f);
  enemy.burstNumber = data.value("burstNumber", 1);
  enemy.burstInterval = data.value("burstInterval", 0.1f);
  reg.get_components<component::Enemy>().insert_at(entity, enemy);
}

/** @} */
} // namespace component
