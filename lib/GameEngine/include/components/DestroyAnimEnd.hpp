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
struct DestroyAnimEnd {};

/**
 * @brief Adds a Dead tag to an entity from its blueprint data.
 * @param reg Registry owning the entity.
 * @param entity Target entity.
 */
inline void destroyAnimEndFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                                   const nlohmann::json & /*data*/) {
  component::DestroyAnimEnd dead{};

  reg.get_components<component::DestroyAnimEnd>().insert_at(entity, dead);
}

/** @} */
} // namespace component
