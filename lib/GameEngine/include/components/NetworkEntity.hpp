#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>

#include <Registry.hpp>
#include <cstdint>

namespace component {
    struct NetworkEntity {
        std::uint32_t id{0};
        std::uint16_t archetypeId{0};
    };

inline void networkEntityFromJson(rtk::ecs::Registry& reg, std::size_t entity, const nlohmann::json& data) {
    component::NetworkEntity networkEntity{};
    networkEntity.id = data.value("id", 0u);
    networkEntity.archetypeId = data.value("archetypeId", static_cast<std::uint16_t>(0));

    reg.get_components<component::NetworkEntity>().insert_at(entity, networkEntity);
}

} // namespace component
