#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>

#include <Registry.hpp>

#include "../utils/vec2.hpp"

namespace component {
/**
 * @addtogroup api_engine
 * @{
 */

/**
 * @brief Position, rotation and scale of an entity in world space.
 *
 * Positions are in pixels, with the origin in the top left corner, x growing
 * to the right and y growing downwards.
 */
struct Transform {
  float rotation;       ///< Rotation in radians.
  utils::vec2 position; ///< Top left corner, in pixels.
  utils::vec2 scale;    ///< Scale factor on each axis.
};

/**
 * @brief Adds a Transform to an entity from its blueprint data.
 * @param reg Registry owning the entity.
 * @param entity Target entity.
 * @param data JSON object with `position`, `rotation` and `scale`.
 */
inline void transformFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                              const nlohmann::json &data) {
  component::Transform transform{};

  if (data.contains("position")) {
    transform.position.x = data["position"].value("x", 0.0f);
    transform.position.y = data["position"].value("y", 0.0f);
  } else {
    transform.position.x = data.value("x", 0.0f);
    transform.position.y = data.value("y", 0.0f);
  }

  transform.rotation = data.value("rotation", 0.0f);

  if (data.contains("scale")) {
    transform.scale.x = data["scale"].value("x", 1.0f);
    transform.scale.y = data["scale"].value("y", 1.0f);
  } else {
    transform.scale.x = 1.0f;
    transform.scale.y = 1.0f;
  }

  reg.get_components<component::Transform>().insert_at(entity, transform);
}

/** @} */
} // namespace component
