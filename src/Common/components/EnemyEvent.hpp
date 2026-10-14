#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>

#include <Registry.hpp>

namespace rtype::events {
struct EnemyEscapedEvent {
  std::size_t entityId;
};

inline void enemyEscapedEventFromJson(rtk::ecs::Registry &reg,
                                      std::size_t entity,
                                      const nlohmann::json &data) {
  rtype::events::EnemyEscapedEvent event{};
  event.entityId = data.value("entityId", entity);

  reg.get_components<rtype::events::EnemyEscapedEvent>().insert_at(entity,
                                                                   event);
}

} // namespace rtype::events

namespace component {

inline void enemyEventFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                               const nlohmann::json &data) {
  rtype::events::EnemyEscapedEvent event{};
  event.entityId = data.value("entityId", entity);

  reg.get_components<rtype::events::EnemyEscapedEvent>().insert_at(entity,
                                                                   event);
}

} // namespace component