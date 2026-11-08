#pragma once

#include "Logger/Logger.hpp"
#include "NetworkIdGenerator.hpp"
#include "Registry.hpp"
#include "components/Transform.hpp"
#include "components/WaveMovement.hpp"
#include "factoryDataDriven/factoryDataDriven.hpp"
#include <ISystem.hpp>
#include <cstdint>
#include <functional>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace rtype::events {
/**
 * @addtogroup api_gameplay
 * @{
 */

/**
 * @brief Published by the server when a new enemy is spawned.
 */
struct EnemySpawnedEvent {
  std::uint32_t networkId;    ///< Network id of the enemy.
  float x;                    ///< Spawn position on the x axis, in pixels.
  float y;                    ///< Spawn position on the y axis, in pixels.
  std::string enemyBlueprint; ///< The enemy who will spawn.
};

/** @} */
} // namespace rtype::events

namespace rtk::systems {
/**
 * @addtogroup api_gameplay
 * @{
 */

/**
 * @brief Spawns random enemies and coordinated squadron waves.
 *
 * Every enemy is data-driven: its behaviour comes from its JSON blueprint.
 * Enemies whose blueprint has a `waveMovement` component can also be spawned
 * as a squadron following the same sine path.
 */
class EnemySpawnerSystem final : public rtk::ecs::ISystem {
private:
  rtk::factory::EntityFactory &_factory;
  busEvent &_eventBus;
  std::function<bool()> _canSpawn;

  struct EnemyPool {
    std::vector<std::string> enemies;
    std::vector<std::string> waveEnemies;
  };

  static inline const std::vector<std::string> _enemiesStage1{
      "enemyBidos",      "enemyMecha", "enemyRedShip",
      "enemySpinner",    "enemyPod",   "enemyDart",
      "enemyArmoredPod", "enemyCargo", "enemyGreenMecha"};
  static inline const std::vector<std::string> _waveEnemiesStage1{
      "enemyRedShip", "enemySpinner", "enemyDart"};

  static inline const std::vector<std::string> _enemiesStage2{
      "enemySeed", "enemyTendril", "enemyShell"};
  static inline const std::vector<std::string> _waveEnemiesStage2{"enemySeed"};
  static inline const std::unordered_map<std::string, EnemyPool> _pools{
      {"stage1", {_enemiesStage1, _waveEnemiesStage1}},
      {"stage2", {_enemiesStage2, _waveEnemiesStage2}},
  };

  const std::string *_poolName{nullptr};

  float _spawnTimer{0.0f};
  float _spawnInterval{2.0f};
  float _waveTimer{0.0f};
  float _waveInterval{10.0f};
  bool _waveActive{false};
  int _waveSize{5};
  int _waveRemaining{0};
  float _waveStepTimer{0.0f};
  float _waveStepDelay{0.35f};
  float _waveBaseY{300.0f};
  std::string _waveBlueprint;

  std::mt19937 _rng;
  std::uniform_real_distribution<float> _distX;
  std::uniform_real_distribution<float> _distY;
  std::uniform_real_distribution<float> _distWaveY;

public:
  static constexpr const char *defaultPool = "stage1";

  /**
   * @brief Creates the spawner.
   * @param factory Factory used to build enemy blueprints.
   * @param eventBus Bus on which EnemySpawnedEvent is published.
   * @param canSpawn Optional predicate, spawning pauses while it returns false.
   * @param poolName Optional name of the pool to spawn from, read every frame
   * so the owner can switch it between stages. `stage1` when null.
   */
  EnemySpawnerSystem(rtk::factory::EntityFactory &factory, busEvent &eventBus,
                     std::function<bool()> canSpawn = nullptr,
                     const std::string *poolName = nullptr)
      : _factory(factory), _eventBus(eventBus), _canSpawn(std::move(canSpawn)),
        _poolName(poolName), _rng(std::random_device{}()),
        _distX(1920.0f, 2400.0f), _distY(200.0f, 500.0f),
        _distWaveY(280.0f, 420.0f) {}

  /**
   * @brief Tells whether a pool with this name exists.
   * @param name Pool name, as written in the stage file.
   * @return True when the pool exists.
   */
  static bool hasPool(const std::string &name) { return _pools.contains(name); }

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

    const EnemyPool *pool = currentPool();
    if (!pool)
      return;

    _spawnTimer += dt;
    if (_spawnTimer >= _spawnInterval) {
      _spawnTimer = 0.0f;
      if (!pool->enemies.empty())
        spawnEnemy(reg, pick(pool->enemies), _distX(_rng), _distY(_rng));
    }

    _waveTimer += dt;
    if (!_waveActive && _waveTimer >= _waveInterval) {
      _waveTimer = 0.0f;
      if (!pool->waveEnemies.empty()) {
        _waveActive = true;
        _waveRemaining = _waveSize;
        _waveStepTimer = _waveStepDelay;
        _waveBaseY = _distWaveY(_rng);
        _waveBlueprint = pick(pool->waveEnemies);
      }
    }

    if (_waveActive) {
      _waveStepTimer += dt;
      if (_waveStepTimer >= _waveStepDelay) {
        _waveStepTimer = 0.0f;
        spawnEnemy(reg, _waveBlueprint, 1950.0f, _waveBaseY);
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
  /**
   * @brief Finds the pool named by the owner, or the default one.
   * @return The pool, or nullptr when the name matches no pool.
   */
  const EnemyPool *currentPool() const {
    const std::string name = _poolName ? *_poolName : defaultPool;
    const auto it = _pools.find(name);
    return it != _pools.end() ? &it->second : nullptr;
  }

  /**
   * @brief Picks a random blueprint name.
   * @param names Non-empty list of blueprint names.
   * @return One of the names.
   */
  const std::string &pick(const std::vector<std::string> &names) {
    std::uniform_int_distribution<std::size_t> dist(0, names.size() - 1);
    return names[dist(_rng)];
  }

  /**
   * @brief Creates an enemy from its blueprint and notifies the network.
   *
   * If the blueprint has a WaveMovement, its wave restarts from the spawn
   * position; amplitude and frequency stay the ones of the JSON.
   */
  void spawnEnemy(rtk::ecs::Registry &reg, const std::string &blueprint,
                  float x, float y) {
    std::uint32_t netId =
        rtype::server::NetworkIdGenerator::getInstance().getNextId();
    auto enemyEntity = _factory.createNetwork(reg, blueprint, netId);
    auto *transform =
        reg.get_components<component::Transform>().get(enemyEntity);
    auto *wave = reg.get_components<component::WaveMovement>().get(enemyEntity);

    if (!transform)
      return;
    transform->position.x = x;
    transform->position.y = y;

    if (wave) {
      wave->baseY = y;
      wave->time = 0.0f;
    }

    _eventBus.publish(rtype::events::EnemySpawnedEvent{
        netId, transform->position.x, transform->position.y, blueprint});
  }
};

/** @} */
} // namespace rtk::systems
