#pragma once

#include "EngineContext.hpp"
#include "Scene/serverGameScene.hpp"
#include "Scene/serverLobbyScene.hpp"
#include "Server.hpp"
#include "collision/collisionBuffer.hpp"
#include "factoryDataDriven/factoryDataDriven.hpp"
#include "sceneManager/sceneManager.hpp"
#include "serverScene.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <unordered_map>

namespace rtype::server {

class Game {
  friend class ServerLobbyScene;
  friend class ServerGameScene;

public:
  explicit Game(std::uint16_t port = 4242);
  ~Game();

  void onLoad(rtk::EngineContext &context);
  void onUpdate(rtk::EngineContext &context, float dt);
  void onUnload(rtk::EngineContext &context);
  bool shouldContinue() const;

  void incrementServerTick() { _serverTick++; }
  std::uint32_t getServerTick() const { return _serverTick; }
  std::size_t getPlayerCount() const { return _players.size(); }
  const std::unordered_map<std::uint32_t, std::size_t> &getPlayers() const {
    return _players;
  }
  bool isGameActive() const { return _isGameActive; }
  void setGameActive(bool active) { _isGameActive = active; }
  void resetGameState() {
    _players.clear();
    _serverTick = 0;
    _isGameActive = false;
  }

private:
  void registerComponents(rtk::EngineContext &context);
  void registerBuilders(rtk::EngineContext &context);
  void loadEntityBlueprints(rtk::EngineContext &context);
  void subscribeEvents(rtk::EngineContext &context);
  void addSystems(rtk::EngineContext &context);

  void handleConnections(rtk::EngineContext &context);
  void handleInputs(rtk::EngineContext &context);
  void handleEntityQueries(rtk::EngineContext &context);
  void cleanupOffscreenEnvironment(rtk::EngineContext &context);
  void sendSnapshots(rtk::EngineContext &context);
  void createBackground(rtk::EngineContext &context);

  float createGate(rtk::EngineContext &context, float startX, bool symmetric);
  float createRamp(rtk::EngineContext &context, float startX, bool symmetric);
  float createReactor(rtk::EngineContext &context, float startX,
                      bool symmetric);
  float createHighGround(rtk::EngineContext &context, float startX,
                         bool symmetric);
  float createLowGround(rtk::EngineContext &context, float startX,
                        bool symmetric);
  float createMediumGround(rtk::EngineContext &context, float startX,
                           bool symmetric);

  void sendExistingEntities(std::uint32_t connectionId,
                            rtk::EngineContext &context);
  void broadcastPlayerCreated(std::uint32_t connectionId,
                              rtk::EngineContext &context);
  void broadcastPlayerDestroyed(std::uint32_t connectionId,
                                std::uint32_t networkId);

  ServerDependency _serverContext;
  ServerInput _serverInput;

  rtk::SceneManager<ServerSceneType, ServerDependency, ServerInput,
                    ServerLobbyScene, ServerGameScene>
      _sceneManager;

  Server _server;
  std::unordered_map<std::uint32_t, std::size_t> _players;
  std::array<std::uint16_t, 4> _playerArchetypes{};
  std::uint32_t _serverTick = 0;
  bool _running = true;
  bool _isGameActive = false;
  rtk::collision::CollisionBuffer _collisionBuffer;
};

} // namespace rtype::server