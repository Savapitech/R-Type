#pragma once

#include "Logger/Logger.hpp"
#include "NetworkIdGenerator.hpp"
#include "components/Transform.hpp"
#include "factoryDataDriven/factoryDataDriven.hpp"
#include <ISystem.hpp>
#include <cstdint>
#include <functional>
#include <random>

namespace rtype::events {
/**
 * @addtogroup api_gameplay
 * @{
 */

/**
 * @brief Published by the server when a new enemy is spawned.
 */
struct EnemySpawnedEvent {
  std::uint32_t networkId; ///< Network id of the enemy.
  float x;                 ///< Spawn position on the x axis, in pixels.
  float y;                 ///< Spawn position on the y axis, in pixels.
};

/** @} */
} // namespace rtype::events

namespace rtk::systems {
/**
 * @addtogroup api_gameplay
 * @{
 */

/**
 * @brief Spawns a Bydos enemy at a random position right of the screen every
 * 1.5 seconds.
 */
class EnemySpawnerSystem final : public rtk::ecs::ISystem {
private:
  rtk::factory::EntityFactory &_factory;
  busEvent &_eventBus;
  std::function<bool()> _canSpawn;

  float _spawnTimer{0.0f};
  float _spawnInterval{1.5f};

  std::mt19937 _rng;
  std::uniform_real_distribution<float> _distX;
  std::uniform_real_distribution<float> _distY;

public:
  /**
   * @brief Creates the spawner.
   * @param factory Factory used to build the `enemyBidos` blueprint.
   * @param eventBus Bus on which EnemySpawnedEvent is published.
   * @param canSpawn Optional predicate, spawning pauses while it returns false.
   */
  EnemySpawnerSystem(rtk::factory::EntityFactory &factory, busEvent &eventBus,
                     std::function<bool()> canSpawn = nullptr)
      : _factory(factory), _eventBus(eventBus), _canSpawn(std::move(canSpawn)),
        _rng(std::random_device{}()), _distX(1920.0f, 2920.0f),
        _distY(200.0f, 400.0f) {}

  /**
   * @brief Logs that the system is loaded.
   */
  void onStart(rtk::ecs::Registry &) override {
    LOG_INFO("Load Enemy Spawner System (Server Side)");
  }

  /**
   * @brief Advances the spawn timer and spawns an enemy when it expires.
   * @param reg Registry receiving the new enemy.
   * @param dt Elapsed time since the last frame, in seconds.
   */
  void update(rtk::ecs::Registry &reg, float dt) override {
    if (_canSpawn && !_canSpawn())
      return;

    _spawnTimer += dt;

    if (_spawnTimer >= _spawnInterval) {
      _spawnTimer = 0.0f;

      std::uint32_t netId =
          rtype::server::NetworkIdGenerator::getInstance().getNextId();

      auto enemyEntity = _factory.createNetwork(reg, "enemyBidos", netId);
      auto *transform =
          reg.get_components<component::Transform>().get(enemyEntity);

      if (transform) {
        transform->position.x = _distX(_rng);
        transform->position.y = _distY(_rng);

        _eventBus.publish(rtype::events::EnemySpawnedEvent{
            netId, transform->position.x, transform->position.y});
      }
    }
  }

  /**
   * @brief Does nothing.
   */
  void onStop(rtk::ecs::Registry &) override {}
};

/** @} */
} // namespace rtk::systems
