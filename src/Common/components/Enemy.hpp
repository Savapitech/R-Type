#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>

#include <Registry.hpp>

namespace component {
struct Enemy {
  float health{100.0f};
};

inline void enemyFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                          const nlohmann::json &data) {
  component::Enemy enemy{};
  enemy.health = data.value("health", 100.0f);

  reg.get_components<component::Enemy>().insert_at(entity, enemy);
}

} // namespace component