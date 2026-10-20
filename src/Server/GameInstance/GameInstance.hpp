#pragma once

#include "EngineContext.hpp"

#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace rtype::server {

  class GameInstance {
    public:
      static constexpr std::size_t NO_ENTITY =
          static_cast<std::size_t>(-1);

      explicit GameInstance(std::uint32_t roomId);

      GameInstance(const GameInstance&) = delete;
      GameInstance& operator=(const GameInstance&) = delete;
      GameInstance(GameInstance&&) = delete;
      GameInstance& operator=(GameInstance&&) = delete;
      std::uint32_t getRoomId() const;
      void addPlayer(std::uint32_t connectionId, std::size_t entity);
      bool removePlayer(std::uint32_t connectionId);
      bool hasPlayer(std::uint32_t connectionId) const;
      std::size_t getPlayerCount() const;
      const std::unordered_map<std::uint32_t, std::size_t>&
      getPlayers() const;
      void incrementServerTick();
      std::uint32_t getServerTick() const;
      bool isActive() const;
      void setGameActive(bool active);
      void resetGameState();
      rtk::EngineContext& getContext();
      void start();
      void update(float dt);
      void stop();

    private:
      std::uint32_t _roomId;
      std::unordered_map<std::uint32_t, std::size_t> _players;
      std::uint32_t _serverTick = 0;
      bool _isActive = false;
      bool _systemsStarted = false;
      rtk::ecs::Registry _registry;
      rtk::ecs::Scheduler _scheduler;
      busEvent _eventBus;
      rtk::EngineContext _context {
          .registry = _registry,
          .scheduler = _scheduler,
          .eventBus = _eventBus
      };
  };
} // namespace rtype::server
