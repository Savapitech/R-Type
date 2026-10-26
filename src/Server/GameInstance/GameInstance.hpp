#pragma once

#include "EngineContext.hpp"
#include "GameInstance/Scene/InstanceScenes.hpp"
#include "Server.hpp"
#include "collision/collisionBuffer.hpp"
#include "sceneManager/sceneManager.hpp"
#include "serverScene.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace rtype::server {

  class GameInstance {
    public:
      static constexpr std::size_t NO_ENTITY =
          static_cast<std::size_t>(-1);

      explicit GameInstance(std::uint32_t roomId, Server &server);

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
      void start(Server &server);
      void update(float dt);
      void stop();
      void initialize();
      const std::array<std::uint16_t, 4> &getPlayerArchetypes() const;
      void addSystems(Server& server);
      void subscribeEvents(Server& server);
      void processInput(const Server::InputEvent& inputEvent, Server& server);
      bool addConnectedPlayer(std::uint32_t connectionId, Server& server);
      void sendExistingEntities(std::uint32_t connectionId, Server& server);
      bool removeConnectedPlayer(std::uint32_t connectionId, Server& server);
      void sendSnapshots(Server& server);
      void processEntityQuery(const Server::QueryEntityEvent &queryEvent, Server &server);
    
    private:
      std::uint32_t _roomId;
      std::unordered_map<std::uint32_t, std::size_t> _players;
      std::uint32_t _serverTick = 0;
      bool _isActive = false;
      bool _systemsStarted = false;
      bool _initialized = false;
      rtk::ecs::Registry _registry;
      rtk::ecs::Scheduler _scheduler;
      busEvent _eventBus;
      rtk::EngineContext _context {
          .registry = _registry,
          .scheduler = _scheduler,
          .eventBus = _eventBus
      };
      GameInstanceDependency _dependency;
      ServerInput _sceneInput;
      rtk::SceneManager<
        ServerSceneType,
        GameInstanceDependency,
        ServerInput,
        InstanceLobbyScene,
        InstanceGameScene
      > _sceneManager;
      
      std::array<std::uint16_t, 4> _playerArchetypes{};
      rtk::collision::CollisionBuffer _collisionBuffer;
      bool _systemsRegistered = false;
      bool _eventsRegistered = false;

      void registerComponents(rtk::EngineContext &context);
      void registerBuilders(rtk::EngineContext &context);
      void loadEntityBlueprints(rtk::EngineContext &context);

  };
} // namespace rtype::server
