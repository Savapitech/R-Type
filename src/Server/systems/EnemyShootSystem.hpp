#pragma once

#include "NetworkIdGenerator.hpp"
#include "Protocol/EntityPayload.hpp"
#include "Server.hpp"
#include "components/Dead.hpp"
#include "components/Enemy.hpp"
#include "components/Transform.hpp"
#include "components/Velocity.hpp"
#include "factoryDataDriven/factoryDataDriven.hpp"
#include "game.hpp"
#include <ISystem.hpp>
#include <cmath>
#include <limits>
#include <numbers>
#include <unordered_map>
#include <vector>

namespace rtk::systems {
/**
 * @addtogroup api_server
 * @{
 */

/**
 * @brief Makes visible enemies fire directed projectiles towards the closest
 * player.
 */
class EnemyShootSystem final : public rtk::ecs::ISystem {
private:
  rtk::factory::EntityFactory &_factory;
  rtype::server::Server &_server;
  const std::unordered_map<std::uint32_t, std::size_t> &_players;
  std::uint32_t _serverTick = 0;

  struct PendingBullet {
    float spawnX;
    float spawnY;
    float velX;
    float velY;
    float rotation;
  };

public:
  /**
   * @brief Creates the system.
   * @param factory Factory used to build the `enemy_bullet` blueprint.
   * @param server Server used to send CreateEntity.
   * @param players Connected players.
   */
  EnemyShootSystem(
      rtk::factory::EntityFactory &factory, rtype::server::Server &server,
      const std::unordered_map<std::uint32_t, std::size_t> &players)
      : _factory(factory), _server(server), _players(players) {}

  ~EnemyShootSystem() override = default;

  /**
   * @brief Does nothing.
   */
  void onStart(rtk::ecs::Registry &) override {}

  /**
   * @brief Counts down each visible enemy fire timer and spawns aimed bullets.
   * @param reg Registry holding the entities.
   * @param dt Elapsed time since the last frame, in seconds.
   */
  void update(rtk::ecs::Registry &reg, float dt) override {
    _serverTick++;

    auto view = reg.view<component::Transform, component::Enemy>();
    auto &transforms = reg.get_components<component::Transform>();
    auto &enemies = reg.get_components<component::Enemy>();
    auto &deads = reg.get_components<component::Dead>();

    std::vector<PendingBullet> bulletsToSpawn;

    for (auto entity : view) {
      if (deads.get(entity) != nullptr) {
        continue;
      }

      auto *transform = transforms.get(entity);
      auto *enemy = enemies.get(entity);

      if (!transform || !enemy) {
        continue;
      }

      const bool isVisible =
          transform->position.x >= 0.0f && transform->position.x <= 1920.0f &&
          transform->position.y >= 0.0f && transform->position.y <= 1080.0f;

      if (!isVisible) {
        continue;
      }

      enemy->fireTimer -= dt;
      if (enemy->fireTimer <= 0.0f) {
        enemy->fireTimer = enemy->fireRate;

        const float spawnX = transform->position.x - 20.0f;
        const float spawnY = transform->position.y + 16.0f;

        float closestDistSq = std::numeric_limits<float>::max();
        const component::Transform *targetPlayer = nullptr;

        for (const auto &[connId, playerEntity] : _players) {
          (void)connId;
          if (playerEntity == rtype::server::Game::NO_ENTITY) {
            continue;
          }
          if (deads.get(playerEntity) != nullptr) {
            continue;
          }
          const auto *playerTransform = transforms.get(playerEntity);
          if (!playerTransform) {
            continue;
          }

          float dx = playerTransform->position.x - spawnX;
          float dy = playerTransform->position.y - spawnY;
          float distSq = dx * dx + dy * dy;
          if (distSq < closestDistSq) {
            closestDistSq = distSq;
            targetPlayer = playerTransform;
          }
        }

        constexpr float bulletSpeed = 350.0f;
        float velX = -bulletSpeed;
        float velY = 0.0f;
        float rotation = std::numbers::pi_v<float>;

        if (targetPlayer) {
          const float dx = targetPlayer->position.x - spawnX;
          const float dy = targetPlayer->position.y - spawnY;

          if (dx < -10.0f) {
            constexpr float maxSlope = 0.8f;
            float slope = dy / -dx;
            if (slope > maxSlope) {
              slope = maxSlope;
            } else if (slope < -maxSlope) {
              slope = -maxSlope;
            }

            const float aimedDirX = -1.0f;
            const float aimedDirY = slope;
            const float length = std::hypot(aimedDirX, aimedDirY);

            velX = (aimedDirX / length) * bulletSpeed;
            velY = (aimedDirY / length) * bulletSpeed;
            rotation = std::atan2(velY, velX);
          }
        }

        bulletsToSpawn.push_back({spawnX, spawnY, velX, velY, rotation});
      }
    }

    for (const auto &bullet : bulletsToSpawn) {
      std::uint32_t bulletNetId =
          rtype::server::NetworkIdGenerator::getInstance().getNextId();
      auto bulletEntity =
          _factory.createNetwork(reg, "enemy_bullet", bulletNetId);

      auto *bulletTransform =
          reg.get_components<component::Transform>().get(bulletEntity);
      auto *bulletVelocity =
          reg.get_components<component::Velocity>().get(bulletEntity);

      if (bulletTransform) {
        bulletTransform->position = {bullet.spawnX, bullet.spawnY};
        bulletTransform->rotation = bullet.rotation;
      }
      if (bulletVelocity) {
        bulletVelocity->x = bullet.velX;
        bulletVelocity->y = bullet.velY;
      }

      if (bulletTransform) {
        rtype::protocol::CreateEntityPayload payload =
            rtype::protocol::createEntityPayload(
                _serverTick, bulletNetId,
                _factory.getArchetypeID("enemy_bullet"),
                bulletTransform->position.x, bulletTransform->position.y,
                bulletTransform->rotation);

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
