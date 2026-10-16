#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>

#include <Registry.hpp>
#include <cstdint>

namespace component {
/**
 * @addtogroup api_engine
 * @{
 */

/**
 * @brief Links a local entity to its replicated counterpart.
 */
struct NetworkEntity {
  std::uint32_t id{0};          ///< Network id assigned by the server.
  std::uint16_t archetypeId{0}; ///< Archetype id of the entity blueprint.
};

/**
 * @brief Adds a NetworkEntity to an entity from its blueprint data.
 * @param reg Registry owning the entity.
 * @param entity Target entity.
 * @param data JSON object with optional `id` and `archetypeId` fields.
 */
inline void networkEntityFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                                  const nlohmann::json &data) {
  component::NetworkEntity networkEntity{};
  networkEntity.id = data.value("id", 0u);
  networkEntity.archetypeId =
      data.value("archetypeId", static_cast<std::uint16_t>(0));

  reg.get_components<component::NetworkEntity>().insert_at(entity,
                                                           networkEntity);
}

/** @} */
} // namespace component
