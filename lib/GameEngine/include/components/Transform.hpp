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

    if (data.contains("position")) {
        transform.position.x = data["position"].value("x", 0.0f);
        transform.position.y = data["position"].value("y", 0.0f);
    } else {
        transform.position.x = data.value("x", 0.0f);
        transform.position.y = data.value("y", 0.0f);
    }

    transform.rotation = data.value("rotation", 0.0f);

    if (data.contains("scale")) {
        transform.scale.x = data["scale"].value("x", 1.0f);
        transform.scale.y = data["scale"].value("y", 1.0f);
    } else {
        transform.scale.x = 1.0f;
        transform.scale.y = 1.0f;
    }

    reg.get_components<component::Transform>().insert_at(entity, transform);
}

} // namespace component