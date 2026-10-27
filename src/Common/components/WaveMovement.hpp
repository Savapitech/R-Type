#pragma once

#include <Registry.hpp>
#include <nlohmann/json.hpp>

namespace component {
/**
 * @addtogroup api_gameplay
 * @{
 */

/**
 * @brief Parameters for sinusoidal wave flight trajectory.
 */
struct WaveMovement {
  float baseY{300.0f};     ///< Center vertical position.
  float amplitude{120.0f}; ///< Vertical oscillation amplitude.
  float frequency{2.5f};   ///< Oscillation frequency in radians per second.
  float time{0.0f};        ///< Elapsed flight time.
};

/**
 * @brief Adds WaveMovement to an entity from its blueprint data.
 * @param reg Registry owning the entity.
 * @param entity Target entity.
 * @param data JSON object with optional `baseY`, `amplitude`, `frequency`
 * fields.
 */
inline void waveMovementFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                                 const nlohmann::json &data) {
  component::WaveMovement wave{};
  wave.baseY = data.value("baseY", 300.0f);
  wave.amplitude = data.value("amplitude", 120.0f);
  wave.frequency = data.value("frequency", 2.5f);
  wave.time = data.value("time", 0.0f);

  reg.get_components<component::WaveMovement>().insert_at(entity, wave);
}

/** @} */
} // namespace component
