#pragma once
#include <Registry.hpp>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>

namespace component {
/**
 * @addtogroup api_gameplay
 * @{
 */

/**
 * @brief Marks an entity can drop a power  up.
 */
struct DropPowerUp {
  std::string blueprintName{"powerup"}; ///< key in the .json file
};

/**
 * @brief Adds a dropPowerUp to an entity from its blueprint data.
 *
 * @param reg Registry owning the entity.
 * @param entity Target entity.
 * @param data JSON object with optional explosion parameters.
 */
inline void dropPowerupFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                                const nlohmann::json &data) {
  component::DropPowerUp drop{};
  drop.blueprintName = data.value("blueprintName", "powerup");
  reg.get_components<component::DropPowerUp>().insert_at(entity, drop);
}
} // namespace component