#pragma once

#include <cstddef>
#include <nlohmann/json.hpp>

#include <Registry.hpp>

namespace component {
/**
 * @addtogroup api_gameplay
 * @{
 */

/**
 * @brief Marks an entity as a part of the stage boss.
 *
 * Used by stages whose `boss` key is `parts`: the boss is the set of stage
 * entities carrying this component, and it is defeated once all of them are
 * destroyed (for instance the four thrusters of the Gigantic Warship).
 */
struct BossPart {};

/**
 * @brief Adds a BossPart to an entity from its blueprint data.
 * @param reg Registry owning the entity.
 * @param entity Target entity.
 */
inline void bossPartFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                             const nlohmann::json &) {
  reg.get_components<component::BossPart>().insert_at(entity,
                                                      component::BossPart{});
}

/** @} */
} // namespace component
