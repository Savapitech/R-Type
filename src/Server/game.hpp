#pragma once

#include <cstdint>
#include <unordered_map>

#include "EngineContext.hpp"
#include "Server.hpp"
#include "collision/collisionBuffer.hpp"

namespace rtype::server {

class Game {
public:
  explicit Game(std::uint16_t port = 4242);
  ~Game();

  void onLoad(rtk::EngineContext &context);
  void onUpdate(rtk::EngineContext &context, float dt);
  bool shouldContinue() const;
  void onUnload(rtk::EngineContext &context);

private:
  void registerComponents(rtk::EngineContext &context);
  void registerBuilders(rtk::EngineContext &context);
  void loadEntityBlueprints(rtk::EngineContext &context);
  void addSystems(rtk::EngineContext &context);
  void subscribeEvents(rtk::EngineContext &context);

  void handleConnections(rtk::EngineContext &context);
  void handleInputs(rtk::EngineContext &context);

  void createBackground(rtk::EngineContext &context);
  float createGate(rtk::EngineContext &context, float startX, bool symmetric);
  float createRamp(rtk::EngineContext &context, float startX, bool symmetric);
  float createReactor(rtk::EngineContext &context, float startX, bool symmetric);
  float createHighGround(rtk::EngineContext &context, float startX, bool symmetric);
  float createLowGround(rtk::EngineContext &context, float startX, bool symmetric);
  float createMediumGround(rtk::EngineContext &context, float startX, bool symmetric);
  void sendExistingEntities(std::uint32_t connectionId, rtk::EngineContext &context);
  void handleEntityQueries(rtk::EngineContext &context);
  void broadcastPlayerCreated(std::uint32_t connectionId, rtk::EngineContext &context);
  void broadcastPlayerDestroyed(std::uint32_t connectionId, std::uint32_t networkId);
  void sendSnapshots(rtk::EngineContext &context);
  void cleanupOffscreenEnvironment(rtk::EngineContext &context);

  std::uint32_t _serverTick = 0;
  Server _server;
  bool _running{true};
  rtk::collision::CollisionBuffer _collisionBuffer;
  std::unordered_map<std::uint32_t, std::size_t> _players;
};

} // namespace rtype::server
