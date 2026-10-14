#pragma once

#include <ISystem.hpp>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include "components/Dead.hpp"
#include "components/NetworkEntity.hpp"
#include "Protocol/EntityPayload.hpp"
#include "Server.hpp"

namespace rtype::server {

class ServerDeathSystem final : public rtk::ecs::ISystem {
public:
  ServerDeathSystem(Server &server, const std::unordered_map<std::uint32_t, std::size_t> &players)
      : _server(server), _players(players) {}

  ~ServerDeathSystem() override = default;

  void onStart(rtk::ecs::Registry &) override {}

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
        protocol::DestroyEntityPayload payload;
        payload.entityId = netEnt->id;
        for (const auto &[playerId, dummy] : _players) {
          (void)dummy;
          _server.sendDestroyEntity(playerId, payload);
        }
      }
      reg.remove_entity_from_all_pools(entity);
      reg.kill_entity(entity);
    }
  }

  void onStop(rtk::ecs::Registry &) override {}

private:
  Server &_server;
  const std::unordered_map<std::uint32_t, std::size_t> &_players;
};

} // namespace rtype::server
