#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>

#include <Registry.hpp>

namespace component {
    struct Dead {};

inline void deadFromJson(rtk::ecs::Registry& reg, std::size_t entity, const nlohmann::json& /*data*/) {
    component::Dead dead{};

    reg.get_components<component::Dead>().insert_at(entity, dead);
}

} // namespace component