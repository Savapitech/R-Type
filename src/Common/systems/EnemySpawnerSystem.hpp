#pragma once
#include <ISystem.hpp>
#include <random>
#include "Logger/Logger.hpp"
#include "components/Transform.hpp"
#include "factoryDataDriven/factoryDataDriven.hpp"

namespace rtk::systems {
class EnemySpawnerSystem final : public rtk::ecs::ISystem {
private:
    rtk::factory::EntityFactory& _factory;
    float _spawnTimer{0.0f};
    float _spawnInterval{1.5f};

    std::mt19937 _rng;
    std::uniform_real_distribution<float> _distX;
    std::uniform_real_distribution<float> _distY;

public:
    EnemySpawnerSystem(rtk::factory::EntityFactory& factory)
        : _factory(factory),
          _rng(std::random_device{}()),
          _distX(1920.0f, 2920.0f),
          _distY(200.0f, 400.0f) {}

    void onStart(rtk::ecs::Registry &) override {
        LOG_INFO("Load Enemy Spawner System");
    }

    void update(rtk::ecs::Registry &reg, float dt) override {
        _spawnTimer += dt;

        if (_spawnTimer >= _spawnInterval) {
            _spawnTimer = 0.0f;

            auto enemyEntity = _factory.createFromBlueprint(reg, "enemyBidos");

            auto* transform = reg.get_components<component::Transform>().get(enemyEntity);

            if (transform) {
                transform->position.x = _distX(_rng);
                transform->position.y = _distY(_rng);
            }
        }
    }

    void onStop(rtk::ecs::Registry &) override {}
};
} // namespace rtk::systems