#pragma once

#include "NetworkIdGenerator.hpp"
#include "components/Controllable.hpp"
#include "components/Dead.hpp"
#include "components/Nest.hpp"
#include "components/Transform.hpp"
#include "components/Velocity.hpp"
#include "events/busEvents.hpp"
#include "factoryDataDriven/factoryDataDriven.hpp"
#include "systems/EnemySpawnerSystem.hpp"
#include <ISystem.hpp>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace rtk::systems {
/**
 * @addtogroup api_gameplay
 * @{
 */

/**
 * @brief Makes visible nests release bursts of enemies towards the players.
 */
class NestSystem final : public rtk::ecs::ISystem {
public:
  /**
   * @brief Creates the system.
   * @param factory Factory used to build the released enemies.
   * @param eventBus Bus on which EnemySpawnedEvent is published.
   */
  NestSystem(rtk::factory::EntityFactory &factory, busEvent &eventBus)
      : _factory(factory), _eventBus(eventBus) {}

  ~NestSystem() override = default;

  /**
   * @brief Does nothing.
   */
  void onStart(rtk::ecs::Registry &) override {}

  /**
   * @brief Counts down each visible nest and releases its enemies.
   * @param reg Registry holding the entities.
   * @param dt Elapsed time since the last frame, in seconds.
   */
  void update(rtk::ecs::Registry &reg, float dt) override {
    auto view = reg.view<component::Transform, component::Nest>();
    auto &transforms = reg.get_components<component::Transform>();
    auto &nests = reg.get_components<component::Nest>();
    auto &deads = reg.get_components<component::Dead>();
    std::vector<PendingSpawn> toSpawn;

    for (auto entity : view) {
      if (deads.get(entity) != nullptr)
        continue;
      const auto *transform = transforms.get(entity);
      auto *nest = nests.get(entity);
      if (!transform || !nest)
        continue;

      if (transform->position.x < 0.0f || transform->position.x > screenWidth)
        continue;

      const auto *target = closestPlayer(reg, transform->position.x);
      if (!target)
        continue;
      const float dx = target->position.x - transform->position.x;
      if (std::abs(dx) > nest->triggerDistance)
        continue;

      nest->timer -= dt;
      if (nest->timer > 0.0f)
        continue;

      if (nest->burstRemaining <= 0)
        nest->burstRemaining = nest->burstCount;

      const float dirX = dx < 0.0f ? -1.0f : 1.0f;
      const float dirY = transform->position.y < screenHeight / 2.0f ? 1.0f
                                                                     : -1.0f;
      toSpawn.push_back({nest->spawnBlueprint,
                         transform->position.x + nest->spawnOffsetX,
                         transform->position.y + nest->spawnOffsetY,
                         dirX * nest->speed * diagonalX,
                         dirY * nest->speed * diagonalY});

      nest->burstRemaining--;
      nest->timer =
          nest->burstRemaining > 0 ? nest->burstInterval : nest->cooldown;
    }

    for (const auto &spawn : toSpawn)
      release(reg, spawn);
  }

  /**
   * @brief Does nothing.
   */
  void onStop(rtk::ecs::Registry &) override {}

private:
  static constexpr float screenWidth = 1280.0f;
  static constexpr float screenHeight = 720.0f;
  static constexpr float diagonalX = 0.6f;
  static constexpr float diagonalY = 0.8f;

  struct PendingSpawn {
    std::string blueprint;
    float x;
    float y;
    float velX;
    float velY;
  };

  rtk::factory::EntityFactory &_factory;
  busEvent &_eventBus;

  /**
   * @brief Finds the alive player closest to the given x position.
   * @param reg Registry holding the entities.
   * @param x Horizontal position to compare with.
   * @return The player transform, or nullptr when no player is alive.
   */
  static const component::Transform *closestPlayer(rtk::ecs::Registry &reg,
                                                   float x) {
    auto &transforms = reg.get_components<component::Transform>();
    auto &deads = reg.get_components<component::Dead>();
    const component::Transform *best = nullptr;
    float bestDist = std::numeric_limits<float>::max();

    for (auto player :
         reg.view<component::Transform, component::Controllable>()) {
      if (deads.get(player) != nullptr)
        continue;
      const auto *transform = transforms.get(player);
      if (!transform)
        continue;
      const float dist = std::abs(transform->position.x - x);
      if (dist < bestDist) {
        bestDist = dist;
        best = transform;
      }
    }
    return best;
  }

  /**
   * @brief Creates a released enemy and notifies the network.
   * @param reg Registry receiving the enemy.
   * @param spawn Blueprint, position and velocity of the enemy.
   */
  void release(rtk::ecs::Registry &reg, const PendingSpawn &spawn) {
    const std::uint32_t netId =
        rtype::server::NetworkIdGenerator::getInstance().getNextId();
    const auto entity = _factory.createNetwork(reg, spawn.blueprint, netId);

    auto *transform = reg.get_components<component::Transform>().get(entity);
    if (!transform)
      return;
    transform->position = {spawn.x, spawn.y};

    if (auto *velocity = reg.get_components<component::Velocity>().get(entity))
      *velocity = {spawn.velX, spawn.velY};

    _eventBus.publish(rtype::events::EnemySpawnedEvent{netId, spawn.x, spawn.y,
                                                       spawn.blueprint});
  }
};

/** @} */
} // namespace rtk::systems
