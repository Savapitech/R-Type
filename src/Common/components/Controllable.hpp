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
 * @brief Marks an entity as a ship driven by player inputs.
 */
struct Controllable {
  float speed{300.0f};         ///< Movement speed in pixels per second.
  bool isChargingShoot{false}; ///< True while the shoot button is held.
};

/**
 * @brief Adds a Controllable to an entity from its blueprint data.
 * @param reg Registry owning the entity.
 * @param entity Target entity.
 * @param data JSON object with optional `speed` and `isChargingShoot` fields.
 */
inline void controllableFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                                 const nlohmann::json &data) {
  component::Controllable controllable{};
  controllable.speed = data.value("speed", 300.0f);
  controllable.isChargingShoot = data.value("isChargingShoot", false);

  reg.get_components<component::Controllable>().insert_at(entity, controllable);
}

/** @} */
} // namespace component
