#include "InstanceScenes.hpp"
#include "../GameInstance.hpp"
#include "../RoomChannels.hpp"
#include "serverScene.hpp"

#include <cstdint>
#include <iostream>
#include <optional>

namespace rtype::server {

void InstanceLobbyScene::onEnter(rtk::EngineContext &) {
  auto &instance = _dependency.instance;
  if (!instance.hasAlivePlayers())
    instance.respawnPlayers();
  std::cout << "[Room " << instance.getRoomId() << "] Lobby started"
            << std::endl;
}

std::optional<ServerSceneType>
InstanceLobbyScene::onUpdate(rtk::EngineContext &, const ServerInput &, float) {
  if (_dependency.instance.consumeGameStartRequest())
    return ServerSceneType::InGame;
  return std::nullopt;
}

void InstanceLobbyScene::onExit(rtk::EngineContext &) {
  std::cout << "[Room " << _dependency.instance.getRoomId()
            << "] Leaving lobby, starting game" << std::endl;
}

void InstanceGameScene::onEnter(rtk::EngineContext &) {
  auto &instance = _dependency.instance;
  instance.setGameActive(true);
  if (!instance.hasAlivePlayers())
    instance.respawnPlayers();
  instance.createBackground();

  for (const auto &[connectionId, _] : instance.getPlayers()) {
    instance.sendExistingEntities(connectionId, _dependency.server);
  }
  std::cout << "[Room " << _dependency.instance.getRoomId() << "] Game started"
            << std::endl;
}

std::optional<ServerSceneType> InstanceGameScene::onUpdate(rtk::EngineContext &,
                                                           const ServerInput &,
                                                           float dt) {

  auto &instance = _dependency.instance;
  if (instance.getPlayerCount() == 0)
    return ServerSceneType::Lobby;
  if (!instance.hasAlivePlayers()) {
    std::cout << "[Room " << instance.getRoomId() << "] Game Over" << std::endl;
    return ServerSceneType::Lobby;
  }
  instance.updateStage(dt);
  if (instance.hasCompletedAllStages()) {
    std::cout << "[Room " << instance.getRoomId() << "] All stages completed"
              << std::endl;
    return ServerSceneType::Lobby;
  }
  return std::nullopt;
}

void InstanceGameScene::onExit(rtk::EngineContext &) {
  auto &instance = _dependency.instance;
  const std::uint32_t roomId = instance.getRoomId();

  std::cout << "[Room " << roomId << "] Leaving game scene" << std::endl;
  instance.endGame(_dependency.server);
  _dependency.events.push(
      RoomEvent{.type = RoomEvent::Type::GameEnded, .roomId = roomId});
}

} // namespace rtype::server
