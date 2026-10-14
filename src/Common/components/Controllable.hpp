#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>

#include <Registry.hpp>

namespace component {
struct Controllable {
  float speed{300.0f};
  bool isChargingShoot{false};
};

inline void controllableFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                                 const nlohmann::json &data) {
  component::Controllable controllable{};
  controllable.speed = data.value("speed", 300.0f);
  controllable.isChargingShoot = data.value("isChargingShoot", false);

  reg.get_components<component::Controllable>().insert_at(entity, controllable);
}

} // namespace component