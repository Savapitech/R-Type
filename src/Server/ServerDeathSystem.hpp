#pragma once

#include "Protocol/EntityPayload.hpp"
#include "Server.hpp"
#include "components/Dead.hpp"
#include "components/NetworkEntity.hpp"
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
                    const std::uint32_t &serverTick)
      : _server(server), _players(players), _serverTick(serverTick) {}

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

    std::vector<std::size_t> toDestroy;
    for (auto entity : view) {
      toDestroy.push_back(entity);
    }

    for (auto entity : toDestroy) {
      auto *netEnt = netEntities.get(entity);
      if (netEnt && netEnt->id != 0) {
        const auto payload = protocol::destroyEntityPayload(
            _serverTick, netEnt->id, protocol::DestroyReason::Collision);
        for (const auto &[playerId, dummy] : _players) {
          _server.sendDestroyEntity(playerId, payload);
        }
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
};

/** @} */
} // namespace rtype::server
