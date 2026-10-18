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
 * @brief Represents the health of an entity.
 */
struct Health {
    float current{100.0f}; ///< Current health points.
    float max{100.0f};     ///< Maximum health points.
};

/**
 * @brief Adds a Health component to an entity from its blueprint data.
 *
 * @param reg Registry owning the entity.
 * @param entity Target entity.
 * @param data JSON object with optional `current` and `max` fields.
 */
inline void healthFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                           const nlohmann::json &data) {
    component::Health health{};
    health.max = data.value("max", 100.0f);
    health.current = data.value("current", health.max);

    reg.get_components<component::Health>().insert_at(entity, health);
}

/** @} */
} // namespace component