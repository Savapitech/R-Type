#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>

#include <Registry.hpp>

namespace component {
    struct Velocity {
        float x{0.0f};
        float y{0.0f};
    };

inline void velocityFromJson(rtk::ecs::Registry& reg, std::size_t entity, const nlohmann::json& data) {
    component::Velocity velocity{};
    velocity.x = data.value("x", 0.0f);
    velocity.y = data.value("y", 0.0f);

    reg.get_components<component::Velocity>().insert_at(entity, velocity);
}

} // namespace component
