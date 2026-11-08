#pragma once

#include <cstddef>
#include <nlohmann/json.hpp>
#include <string>

#include <Registry.hpp>

namespace component {
/**
 * @addtogroup api_gameplay
 * @{
 */

/**
 * @brief Marks an entity as a nest that releases bursts of enemies.
 *
 * Once a player is within `triggerDistance`, the nest releases `burstCount`
 * enemies, one every `burstInterval` seconds, then waits `cooldown` seconds
 * before the next burst. A nest on the floor sends them upwards, a nest on the
 * ceiling sends them downwards, always diagonally towards the player side.
 */
struct Nest {
  std::string spawnBlueprint{"gouger"}; ///< Blueprint of the released enemy.
  int burstCount{3};                    ///< Enemies released per burst.
  float burstInterval{0.35f};    ///< Delay between two enemies of a burst.
  float cooldown{2.5f};          ///< Delay between two bursts.
  float triggerDistance{900.0f}; ///< Horizontal distance that wakes the nest.
  float speed{260.0f};           ///< Speed of the released enemies.
  float spawnOffsetX{0.0f};      ///< Spawn offset from the nest, x axis.
  float spawnOffsetY{0.0f};      ///< Spawn offset from the nest, y axis.
  int burstRemaining{0};         ///< Enemies left in the current burst.
  float timer{0.0f};             ///< Time before the next release.
};

/**
 * @brief Adds a Nest to an entity from its blueprint data.
 * @param reg Registry owning the entity.
 * @param entity Target entity.
 * @param data JSON object with optional fields.
 */
inline void nestFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                         const nlohmann::json &data) {
  component::Nest nest{};
  nest.spawnBlueprint = data.value("spawnBlueprint", "gouger");
  nest.burstCount = data.value("burstCount", 3);
  nest.burstInterval = data.value("burstInterval", 0.35f);
  nest.cooldown = data.value("cooldown", 2.5f);
  nest.triggerDistance = data.value("triggerDistance", 900.0f);
  nest.speed = data.value("speed", 260.0f);
  nest.spawnOffsetX = data.value("spawnOffsetX", 0.0f);
  nest.spawnOffsetY = data.value("spawnOffsetY", 0.0f);
  reg.get_components<component::Nest>().insert_at(entity, nest);
}

/** @} */
} // namespace component
