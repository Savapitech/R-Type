#pragma once
#include <ISystem.hpp>
#include <random>
#include <cstdint>
#include "Logger/Logger.hpp"
#include "components/Transform.hpp"
#include "factoryDataDriven/factoryDataDriven.hpp"
#include "NetworkIdGenerator.hpp"

namespace rtype::events {
    struct EnemySpawnedEvent {
        std::uint32_t networkId;
        float x;
        float y;
    };
}

namespace rtk::systems {
class EnemySpawnerSystem final : public rtk::ecs::ISystem {
private:
    rtk::factory::EntityFactory& _factory;
    busEvent& _eventBus;

    float _spawnTimer{0.0f};
    float _spawnInterval{1.5f};

    std::mt19937 _rng;
    std::uniform_real_distribution<float> _distX;
    std::uniform_real_distribution<float> _distY;

public:
    EnemySpawnerSystem(rtk::factory::EntityFactory& factory, busEvent& eventBus)
        : _factory(factory),
          _eventBus(eventBus),
          _rng(std::random_device{}()),
          _distX(1920.0f, 2920.0f),
          _distY(200.0f, 400.0f) {}

    void onStart(rtk::ecs::Registry &) override {
        LOG_INFO("Load Enemy Spawner System (Server Side)");
    }

    void update(rtk::ecs::Registry &reg, float dt) override {
        _spawnTimer += dt;

        if (_spawnTimer >= _spawnInterval) {
            _spawnTimer = 0.0f;

            std::uint32_t netId = rtype::server::NetworkIdGenerator::getInstance().getNextId();

            auto enemyEntity = _factory.createNetwork(reg, "enemyBidos", netId);
            auto* transform = reg.get_components<component::Transform>().get(enemyEntity);

            if (transform) {
                transform->position.x = _distX(_rng);
                transform->position.y = _distY(_rng);

                _eventBus.publish(rtype::events::EnemySpawnedEvent{
                    netId, 
                    transform->position.x, 
                    transform->position.y
                });
            }
        }
    }

    void onStop(rtk::ecs::Registry &) override {}
};
} // namespace rtk::systems