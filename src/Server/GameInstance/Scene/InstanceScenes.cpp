#include "InstanceScenes.hpp"
#include "../GameInstance.hpp"
#include "serverScene.hpp"
#include <iostream>
#include <optional>

namespace rtype::server {

  void InstanceLobbyScene::onEnter(rtk::EngineContext &) {
    std::cout << "[Room " << _dependency.instance.getRoomId() << "] Lobby started" << std::endl;
  }

  std::optional<ServerSceneType> InstanceLobbyScene::onUpdate(rtk::EngineContext &, const ServerInput &, float dt) {

    auto &instance = _dependency.instance;
    const auto nbPlayer = instance.getPlayerCount();
    if (nbPlayer == 0) {
      _lobbyTimer = 0.0f;
      return std::nullopt;
    }
    if (nbPlayer >= 4)
      return ServerSceneType::InGame;
    _lobbyTimer += dt;

    if (_lobbyTimer >= 30.0f)
      return ServerSceneType::InGame;
    return std::nullopt;
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
