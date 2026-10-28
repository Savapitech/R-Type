#include "InstanceScenes.hpp"
#include "../GameInstance.hpp"
#include "Protocol/RoomPayload.hpp"
#include "Room/Room.hpp"
#include "serverScene.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>

namespace rtype::server {

  void InstanceLobbyScene::onEnter(rtk::EngineContext &) {
    std::cout << "[Room " << _dependency.instance.getRoomId() << "] Lobby started" << std::endl;
  }

  std::optional<ServerSceneType> InstanceLobbyScene::onUpdate(rtk::EngineContext &, const ServerInput &, float dt) {

    auto &instance = _dependency.instance;
    auto &roomManager = _dependency.roomManager;
    const std::uint32_t roomId = instance.getRoomId();
    auto *room = roomManager.getRoom(roomId);
    if (!room)
      return std::nullopt;
    const std::size_t nbPlayer = room->getPlayers().size();
    if (nbPlayer < 1)
      return std::nullopt;
    if (!roomManager.areAllPlayersReady(roomId))
      return std::nullopt;
    room->setState(RoomState::Playing);
    protocol::GameStartingPayload payload{};
    payload.roomId = roomId;
    for (const auto connectionId : room->getPlayers()) {
      _dependency.server.sendGameStarting(connectionId, payload);
    }
    std::cout << "[Room " << roomId << "] All players ready. Starting game!" << std::endl;
    return ServerSceneType::InGame;
  }

  void InstanceLobbyScene::onExit(rtk::EngineContext&) {
    std::cout << "[Room " << _dependency.instance.getRoomId() << "] Leaving lobby" << std::endl;
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
    std::cout << "[Room " << _dependency.instance.getRoomId() << "] Game started" << std::endl;
  }

  std::optional<ServerSceneType> InstanceGameScene::onUpdate(rtk::EngineContext &, const ServerInput &, float dt) {

    auto &instance = _dependency.instance;
    if (instance.getPlayerCount() == 0) 
        return ServerSceneType::Lobby;
    if (!instance.hasAlivePlayers()) {
        std::cout << "[Room " << instance.getRoomId() << "] Game Over" << std::endl;
        return ServerSceneType::Lobby;
    }
    instance.updateStage(dt);
    if (instance.hasCompletedAllStages()) {
        std::cout << "[Room " << instance.getRoomId() << "] All stages completed" << std::endl;
        return ServerSceneType::Lobby;
    }
    return std::nullopt;
}

  void InstanceGameScene::onExit(rtk::EngineContext&) {
    _dependency.instance.setGameActive(false);
    std::cout << "[Room " << _dependency.instance.getRoomId() << "] Leaving game" << std::endl;
    _dependency.instance.endGame(_dependency.server);
  }

} //namespace rtype::server
