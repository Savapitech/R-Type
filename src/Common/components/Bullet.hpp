#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>

#include <Registry.hpp>

namespace component {
    struct Bullet {
        float damage{10.0f};
    };

inline void bulletFromJson(rtk::ecs::Registry& reg, std::size_t entity, const nlohmann::json& data) {
    component::Bullet bullet{};
    bullet.damage = data.value("damage", 10.0f);

    reg.get_components<component::Bullet>().insert_at(entity, bullet);
}

} // namespace component