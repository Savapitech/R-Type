#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>

#include <Registry.hpp>

#include "../utils/vec2.hpp"

namespace component {
struct Transform {
  float rotation;
  utils::vec2 position;
  utils::vec2 scale;
};

inline void transformFromJson(rtk::ecs::Registry& reg, std::size_t entity, const nlohmann::json& data) {
    component::Transform transform{};
    transform.position.x = data.value("x", 0.0f);
    transform.position.y = data.value("y", 0.0f);

    reg.get_components<component::Transform>().insert_at(entity, transform);
}

} // namespace component