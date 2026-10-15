#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>

#include <Registry.hpp>

namespace rtype::events {
/**
 * @addtogroup api_gameplay
 * @{
 */

/**
 * @brief Published when an enemy leaves the screen on the left side.
 */
struct EnemyEscapedEvent {
  std::size_t entityId; ///< Entity that escaped.
};

/**
 * @brief Attaches an EnemyEscapedEvent to an entity from blueprint data.
 * @param reg Registry owning the entity.
 * @param entity Target entity.
 * @param data JSON object with an optional `entityId` field.
 */
inline void enemyEscapedEventFromJson(rtk::ecs::Registry &reg,
                                      std::size_t entity,
                                      const nlohmann::json &data) {
  rtype::events::EnemyEscapedEvent event{};
  event.entityId = data.value("entityId", entity);

  reg.get_components<rtype::events::EnemyEscapedEvent>().insert_at(entity,
                                                                   event);
}

/** @} */
} // namespace rtype::events

namespace component {
/**
 * @addtogroup api_gameplay
 * @{
 */

/**
 * @brief Alias of rtype::events::enemyEscapedEventFromJson() for blueprints.
 * @param reg Registry owning the entity.
 * @param entity Target entity.
 * @param data JSON object with an optional `entityId` field.
 */
inline void enemyEventFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                               const nlohmann::json &data) {
  rtype::events::EnemyEscapedEvent event{};
  event.entityId = data.value("entityId", entity);

  reg.get_components<rtype::events::EnemyEscapedEvent>().insert_at(entity,
                                                                   event);
}

/** @} */
} // namespace component
