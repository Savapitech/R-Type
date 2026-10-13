#pragma once
#include <vector>

#include <ISystem.hpp>

#include "events/busEvents.hpp"
#include "components/Transform.hpp"

#include "components/Enemy.hpp"
#include "components/EnemyEvent.hpp"

namespace rtk::systems {
class EnemyGCSystem final : public rtk::ecs::ISystem {
public:
    EnemyGCSystem(busEvent &eventBus) : _eventBus(eventBus){};
    ~EnemyGCSystem() override = default;

    void onStart(rtk::ecs::Registry &) override {}

    void update(rtk::ecs::Registry &reg, float) override {
        auto view = reg.view<component::Transform, component::Enemy>();
        auto &transforms = reg.get_components<component::Transform>();

        std::vector<std::size_t> entitiesToDestroy;

        for (auto entity : view) {
            auto *transform = transforms.get(entity);
            if (!transform) continue;

            if (transform->position.x < -100.0f) {
                entitiesToDestroy.push_back(entity);
            }
        }

        for (auto entity : entitiesToDestroy) {
            reg.kill_entity(entity);
            _eventBus.publish(rtype::events::EnemyEscapedEvent{entity});
        }
    }

    void onStop(rtk::ecs::Registry &) override {}

    private:
        busEvent &_eventBus;
};
} // namespace rtk::systems