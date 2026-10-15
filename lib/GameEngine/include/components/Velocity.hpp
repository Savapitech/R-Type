#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>

#include <Registry.hpp>

namespace component {
/**
 * @addtogroup api_engine
 * @{
 */

/**
 * @brief Linear speed of an entity.
 */
struct Velocity {
  float x{0.0f}; ///< Horizontal speed in pixels per second.
  float y{0.0f}; ///< Vertical speed in pixels per second.
};

/**
 * @brief Adds a Velocity to an entity from its blueprint data.
 * @param reg Registry owning the entity.
 * @param entity Target entity.
 * @param data JSON object with optional `x` and `y` fields.
 */
inline void velocityFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                             const nlohmann::json &data) {
  component::Velocity velocity{};
  velocity.x = data.value("x", 0.0f);
  velocity.y = data.value("y", 0.0f);

  reg.get_components<component::Velocity>().insert_at(entity, velocity);
}

/** @} */
} // namespace component
