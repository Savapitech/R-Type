#pragma once

#include <Registry.hpp>
#include <cstdint>
#include <nlohmann/json.hpp>

#include "systems/RenderConcept.hpp"

namespace component {
/**
 * @addtogroup api_engine
 * @{
 */

/**
 * @brief Component representing an individual particle state and lifetime.
 */
struct Particle {
  float lifetime = 0.5f;        ///< Remaining lifetime in seconds.
  float initialLifetime = 0.5f; ///< Total lifetime in seconds.
  rtk::ecs::concepts::ColorRGBA8 initialColor{255, 255, 255,
                                              255}; ///< Base color.
  bool fadeAlpha = true; ///< Whether to fade alpha channel over time.
};

/**
 * @brief Deserializes a Particle component from JSON data.
 * @param reg ECS Registry owning the entity.
 * @param entity Target entity ID.
 * @param data JSON description.
 */
inline void particleFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                             const nlohmann::json &data) {
  component::Particle particle{};
  particle.lifetime = data.value("lifetime", 0.5f);
  particle.initialLifetime = particle.lifetime;
  particle.fadeAlpha = data.value("fadeAlpha", true);

  if (data.contains("color")) {
    particle.initialColor.r = data["color"].value<uint8_t>("r", 255);
    particle.initialColor.g = data["color"].value<uint8_t>("g", 255);
    particle.initialColor.b = data["color"].value<uint8_t>("b", 255);
    particle.initialColor.a = data["color"].value<uint8_t>("a", 255);
  }

  reg.get_components<component::Particle>().insert_at(entity, particle);
}

/** @} */
} // namespace component
