#pragma once

#include "NetworkIdGenerator.hpp"
#include "Protocol/EntityPayload.hpp"
#include "Server.hpp"
#include "components/Enemy.hpp"
#include "components/Transform.hpp"
#include "factoryDataDriven/factoryDataDriven.hpp"
#include <ISystem.hpp>
#include <unordered_map>
#include <vector>

namespace rtk::systems {
/**
 * @addtogroup api_server
 * @{
 */

/**
 * @brief Makes every enemy fire at its own rate, and tell to every players
 * about each new enemy bullet with CreateEntity
 */
class EnemyShootSystem final : public rtk::ecs::ISystem {
private:
  rtk::factory::EntityFactory &_factory;
  rtype::server::Server &_server;
  const std::unordered_map<std::uint32_t, std::size_t> &_players;
  std::uint32_t _serverTick = 0;

  struct PendingBullet {
    float x;
    float y;
  };

public:
  /**
   * @brief Creates the system.
   * @param factory Factory used to build the `enemy_bullet` blueprint.
   * @param server Server used to send CreateEntity.
   * @param players Connected players
   */
  EnemyShootSystem(
      rtk::factory::EntityFactory &factory, rtype::server::Server &server,
      const std::unordered_map<std::uint32_t, std::size_t> &players)
      : _factory(factory), _server(server), _players(players) {}

  /**
   * @brief Does nothing.
   */
  void onStart(rtk::ecs::Registry &) override {}

  /**
   * @brief Counts down each enemy fire timer and spawns a bullet when it
   * runs out.
   * @param reg Registry holding the entities.
   * @param dt Elpsed time since the last frame, in seconds
   */
  void update(rtk::ecs::Registry &reg, float dt) override {
    _serverTick++;

    auto view = reg.view<component::Transform, component::Enemy>();
    auto &transforms = reg.get_components<component::Transform>();
    auto &enemies = reg.get_components<component::Enemy>();

    std::vector<PendingBullet> bulletsToSpawn;

    for (auto entity : view) {
      auto *transform = transforms.get(entity);
      auto *enemy = enemies.get(entity);

      if (!transform || !enemy)
        continue;

      enemy->fireTimer -= dt;
      if (enemy->fireTimer <= 0.0f) {
        enemy->fireTimer = enemy->fireRate;

        bulletsToSpawn.push_back(
            {transform->position.x - 30.0f, transform->position.y + 32});
      }
    }

    for (const auto &bullet : bulletsToSpawn) {
      std::uint32_t bulletNetId =
          rtype::server::NetworkIdGenerator::getInstance().getNextId();
      auto bulletEntity =
          _factory.createNetwork(reg, "enemy_bullet", bulletNetId);

      auto *bulletTransform =
          reg.get_components<component::Transform>().get(bulletEntity);
      if (bulletTransform) {
        bulletTransform->position = {bullet.x, bullet.y + 32};

        rtype::protocol::CreateEntityPayload payload =
            rtype::protocol::createEntityPayload(
                _serverTick, bulletNetId,
                _factory.getArchetypeID("enemy_bullet"),
                bulletTransform->position.x, bulletTransform->position.y);

        for (const auto &[playerId, dummy] : _players) {
          _server.sendCreateEntity(playerId, payload);
        }
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
