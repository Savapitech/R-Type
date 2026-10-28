#pragma once

#include "NetworkIdGenerator.hpp"
#include "Protocol/EntityPayload.hpp"
#include "Server.hpp"
#include "components/Dead.hpp"
#include "components/NetworkEntity.hpp"
#include "components/Transform.hpp"
#include "components/dropPowerUp.hpp"
#include "factoryDataDriven/factoryDataDriven.hpp"
#include "game.hpp"
#include <ISystem.hpp>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace rtype::server {
/**
 * @addtogroup api_server
 * @{
 */

/**
 * @brief Destroys dead entities and tells every player with DestroyEntity.
 */
class ServerDeathSystem final : public rtk::ecs::ISystem {
public:
  /**
   * @brief Creates the system.
   * @param server Server used to send DestroyEntity.
   * @param players Connected players, keyed by connection id.
   * @param serverTick Current server tick.
   */
  ServerDeathSystem(Server &server,
                    std::unordered_map<std::uint32_t, std::size_t> &players,
                    const std::uint32_t &serverTick,
                    rtk::factory::EntityFactory &factory)
      : _server(server), _players(players), _serverTick(serverTick),
        _factory(factory) {}

  ~ServerDeathSystem() override = default;

  /**
   * @brief Does nothing.
   */
  void onStart(rtk::ecs::Registry &) override {}

  /**
   * @brief Broadcasts and destroys every entity tagged with Dead.
   * @param reg Registry holding the entities.
   */
  void update(rtk::ecs::Registry &reg, float) override {
    auto view = reg.view<component::Dead>();
    auto &netEntities = reg.get_components<component::NetworkEntity>();
    auto &transforms = reg.get_components<component::Transform>();
    auto &drops = reg.get_components<component::DropPowerUp>();

    std::vector<std::size_t> toDestroy;
    for (auto entity : view) {
      toDestroy.push_back(entity);
    }

    for (auto entity : toDestroy) {
      auto *netEnt = netEntities.get(entity);
      auto *transform = transforms.get(entity);
      auto *drop = drops.get(entity);

      if (drop && transform) {
        std::uint32_t netId = NetworkIdGenerator::getInstance().getNextId();
        auto powerUpEntity =
            _factory.createNetwork(reg, drop->blueprintName, netId);
        auto *powerUpTransform =
            reg.get_components<component::Transform>().get(powerUpEntity);
        if (powerUpTransform) {
          powerUpTransform->position = transform->position;
          const auto createPayload = protocol::CreateEntityPayload(
              _serverTick, netId, _factory.getArchetypeID(drop->blueprintName),
              powerUpTransform->position.x, powerUpTransform->position.y, 0.0f);
          for (const auto &[playerId, _] : _players) {
            _server.sendCreateEntity(playerId, createPayload);
          }
        }
      }
      if (netEnt && netEnt->id != 0) {
        const auto payload = protocol::destroyEntityPayload(
            _serverTick, netEnt->id, protocol::DestroyReason::Collision);
        for (const auto &[playerId, _] : _players)
          _server.sendDestroyEntity(playerId, payload);
      }
      for (auto &[playerId, playerEntity] : _players) {
        if (playerEntity == entity)
          playerEntity = Game::NO_ENTITY;
      }
      reg.remove_entity_from_all_pools(entity);
      reg.kill_entity(entity);
    }
  }

  /**
   * @brief Does nothing.
   */
  void onStop(rtk::ecs::Registry &) override {}

private:
  Server &_server;
  std::unordered_map<std::uint32_t, std::size_t> &_players;
  const std::uint32_t &_serverTick;
  rtk::factory::EntityFactory &_factory;
};

/** @} */
} // namespace rtype::server
