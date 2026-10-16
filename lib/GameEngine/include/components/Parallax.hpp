#pragma once

#include <Registry.hpp>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace component {
/**
 * @addtogroup api_engine
 * @{
 */

/**
 * @brief Background layer that scrolls left and wraps around.
 */
struct Parallax {
  float speedX{0.0f};    ///< Scrolling speed in pixels per second.
  float loopWidth{0.0f}; ///< Width of one tile of the layer, in pixels.
};

/**
 * @brief Adds a Parallax to an entity from its blueprint data.
 * @param reg Registry owning the entity.
 * @param entity Target entity.
 * @param data JSON object with optional `speedX` and `loopWidth` fields.
 */
inline void parallaxFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                             const nlohmann::json &data) {
  component::Parallax parallax{};
  parallax.speedX = data.value("speedX", 50.0f);
  parallax.loopWidth = data.value("loopWidth", 1920.0f);

  reg.get_components<component::Parallax>().insert_at(entity, parallax);
}

/** @} */
} // namespace component
