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
 * @brief Death effect of an entity.
 */
struct DeathEffect {
  std::string blueprintName{"explosion"}; ///< Blueprint of the death effect.
};

/**
 * @brief Adds a death effect to an entity from its blueprint data.
 * @param reg Registry owning the entity.
 * @param entity Target entity.
 * @param data JSON object with an optional `damage` field.
 */
inline void deathEffectFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                                const nlohmann::json &data) {
  component::DeathEffect effect{};
  effect.blueprintName = data.value("blueprintName", "explosion");

  reg.get_components<component::DeathEffect>().insert_at(entity, effect);
}

/** @} */
} // namespace component
