#pragma once
#include <Registry.hpp>
#include <nlohmann/json.hpp>
#include <string>

namespace component {
/**
 * @addtogroup api_gameplay
 * @{
 */

/**
 * @brief Represents the different types of power up.
 */
enum class powerUpType { Heal, Speed, Shield };

/**
 * @brief Represents the power up.
 */
struct powerUp {
  powerUpType type{powerUpType::Heal}; ///< The type of the power up.
  float value{10.0f};                  ///< The value associated to the type.
};

/**
 * @brief Adds a power up component to an entity from its blueprint data.
 *
 * @param reg Registry owning the entity.
 * @param entity Target entity.
 * @param data JSON object with optional `current` and `max` fields.
 */
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