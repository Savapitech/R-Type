#pragma once
#include <Registry.hpp>
#include <nlohmann/json.hpp>
#include <string>

namespace component {
enum class powerUpType { Heal, Speed, Shield };

struct powerUp {
  powerUpType type{powerUpType::Heal};
  float value{10.0f};
};

inline void powerupFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                            const nlohmann::json &data) {
  component::powerUp pu{};
  std::string typeStr = data.value("type", "heal");

  if (typeStr == "speed")
    pu.type = powerUpType::Speed;
  else if (typeStr == "shield")
    pu.type = powerUpType::Shield;
  else
    pu.type = powerUpType::Heal;

  pu.value = data.value("value", 20.0f);
  reg.get_components<component::powerUp>().insert_at(entity, pu);
}
} // namespace component