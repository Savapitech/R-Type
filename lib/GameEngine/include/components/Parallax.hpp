#pragma once

#include <Registry.hpp>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace component {
struct Parallax {
  float speedX{0.0f};
  float loopWidth{0.0f};
};

inline void parallaxFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                             const nlohmann::json &data) {
  component::Parallax parallax{};
  parallax.speedX = data.value("speedX", 50.0f);
  parallax.loopWidth = data.value("loopWidth", 1920.0f);

  reg.get_components<component::Parallax>().insert_at(entity, parallax);
}

} // namespace component