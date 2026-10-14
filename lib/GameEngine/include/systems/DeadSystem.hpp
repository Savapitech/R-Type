#pragma once
#include <ISystem.hpp>
#include <vector>
#include "components/Dead.hpp"

namespace rtk::systems {
class DeathSystem final : public rtk::ecs::ISystem {
public:
    DeathSystem() = default;
    ~DeathSystem() override = default;

    void onStart(rtk::ecs::Registry &) override {}

    void update(rtk::ecs::Registry &reg, float) override {
        auto view = reg.view<component::Dead>();

        std::vector<std::size_t> toDestroy;
        for (auto entity : view) {
            toDestroy.push_back(entity);
        }

        for (auto entity : toDestroy) {
            reg.remove_entity_from_all_pools(entity);
            reg.kill_entity(entity);
        }
    }

    void onStop(rtk::ecs::Registry &) override {}
};
} // namespace rtk::systems