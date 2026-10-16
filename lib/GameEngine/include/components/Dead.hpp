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
 * @brief Tag marking an entity for destruction at the end of the frame.
 */
struct Dead {};

/**
 * @brief Adds a Dead tag to an entity from its blueprint data.
 * @param reg Registry owning the entity.
 * @param entity Target entity.
 */
inline void deadFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                         const nlohmann::json & /*data*/) {
  component::Dead dead{};

  reg.get_components<component::Dead>().insert_at(entity, dead);
}

/** @} */
} // namespace component
