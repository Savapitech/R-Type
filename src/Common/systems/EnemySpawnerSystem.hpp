#pragma once

#include "Logger/Logger.hpp"
#include "NetworkIdGenerator.hpp"
#include "components/Transform.hpp"
#include "components/WaveMovement.hpp"
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
 * @brief Spawns Bydos enemies and coordinated red ship squadron waves.
 */
class EnemySpawnerSystem final : public rtk::ecs::ISystem {
private:
  rtk::factory::EntityFactory &_factory;
  busEvent &_eventBus;
  std::function<bool()> _canSpawn;

  float _bidosTimer{0.0f};
  float _bidosInterval{2.5f};

  float _waveTimer{0.0f};
  float _waveInterval{10.0f};
  bool _waveActive{false};
  int _waveRemaining{0};
  float _waveStepTimer{0.0f};
  float _waveStepDelay{0.35f};
  float _waveBaseY{300.0f};
  float _waveAmplitude{120.0f};
  float _waveFrequency{2.5f};

  std::mt19937 _rng;
  std::uniform_real_distribution<float> _distX;
  std::uniform_real_distribution<float> _distY;
  std::uniform_real_distribution<float> _distWaveY;

public:
  /**
   * @brief Creates the spawner.
   * @param factory Factory used to build enemy blueprints.
   * @param eventBus Bus on which EnemySpawnedEvent is published.
   * @param canSpawn Optional predicate, spawning pauses while it returns false.
   */
  EnemySpawnerSystem(rtk::factory::EntityFactory &factory, busEvent &eventBus,
                     std::function<bool()> canSpawn = nullptr)
      : _factory(factory), _eventBus(eventBus), _canSpawn(std::move(canSpawn)),
        _rng(std::random_device{}()), _distX(1920.0f, 2400.0f),
        _distY(200.0f, 500.0f), _distWaveY(280.0f, 420.0f) {}

  /**
   * @brief Logs that the system is loaded.
   */
  void onStart(rtk::ecs::Registry &) override {
    LOG_INFO("Load Enemy Spawner System (Server Side)");
  }

  /**
   * @brief Advances spawn timers and spawns enemies and wave formations.
   * @param reg Registry receiving the new enemies.
   * @param dt Elapsed time since the last frame, in seconds.
   */
  void update(rtk::ecs::Registry &reg, float dt) override {
    if (_canSpawn && !_canSpawn())
      return;

    _bidosTimer += dt;
    if (_bidosTimer >= _bidosInterval) {
      _bidosTimer = 0.0f;
      spawnBidos(reg);
    }

    _waveTimer += dt;
    if (!_waveActive && _waveTimer >= _waveInterval) {
      _waveTimer = 0.0f;
      _waveActive = true;
      _waveRemaining = 5;
      _waveStepTimer = _waveStepDelay;
      _waveBaseY = _distWaveY(_rng);
      _waveAmplitude = 80.0f;
      _waveFrequency = 2.0f;
    }

    if (_waveActive) {
      _waveStepTimer += dt;
      if (_waveStepTimer >= _waveStepDelay) {
        _waveStepTimer = 0.0f;
        spawnRedShip(reg, _waveBaseY, _waveAmplitude, _waveFrequency);
        _waveRemaining--;
        if (_waveRemaining <= 0) {
          _waveActive = false;
        }
      }
    }
  }

  /**
   * @brief Does nothing.
   */
  void onStop(rtk::ecs::Registry &) override {}

private:
  void spawnBidos(rtk::ecs::Registry &reg) {
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

  void spawnRedShip(rtk::ecs::Registry &reg, float baseY, float amplitude,
                    float frequency) {
    std::uint32_t netId =
        rtype::server::NetworkIdGenerator::getInstance().getNextId();

    auto enemyEntity = _factory.createNetwork(reg, "enemyRedShip", netId);
    auto *transform =
        reg.get_components<component::Transform>().get(enemyEntity);
    auto *wave = reg.get_components<component::WaveMovement>().get(enemyEntity);

    if (transform) {
      transform->position.x = 1950.0f;
      transform->position.y = baseY;

      if (wave) {
        wave->baseY = baseY;
        wave->amplitude = amplitude;
        wave->frequency = frequency;
        wave->time = 0.0f;
      }

      _eventBus.publish(rtype::events::EnemySpawnedEvent{
          netId, transform->position.x, transform->position.y});
    }
  }
};

/** @} */
} // namespace rtk::systems
