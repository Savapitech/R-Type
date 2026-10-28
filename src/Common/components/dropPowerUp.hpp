#pragma once
#include <Registry.hpp>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>

namespace component {
struct DropPowerUp {
  std::string blueprintName{"powerup"};
};

inline void dropPowerupFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                                const nlohmann::json &data) {
  component::DropPowerUp drop{};
  drop.blueprintName = data.value("blueprint", "powerup");
  reg.get_components<component::DropPowerUp>().insert_at(entity, drop);
}
} // namespace component