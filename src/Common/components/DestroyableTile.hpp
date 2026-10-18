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
 * @brief Marks an entity as a destroyable tile, potentially explosive.
 */
struct DestroyableTile {
    float explosionDamage{20.0f}; ///< Damage dealt to surrounding entities.
    bool isTriggered{false};      ///< Flag to know if the explosion animation/effect started.
};

/**
 * @brief Adds a DestroyableTile to an entity from its blueprint data.
 *
 * @param reg Registry owning the entity.
 * @param entity Target entity.
 * @param data JSON object with optional explosion parameters.
 */
inline void destroyableTileFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                                    const nlohmann::json &data) {
    component::DestroyableTile tile{};
    tile.explosionDamage = data.value("explosionDamage", 20.0f);
    tile.isTriggered = data.value("isTriggered", false);

    reg.get_components<component::DestroyableTile>().insert_at(entity, tile);
}

/** @} */
} // namespace component