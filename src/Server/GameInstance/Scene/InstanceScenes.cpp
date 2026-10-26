#include "InstanceScenes.hpp"
#include "../GameInstance.hpp"
#include "serverScene.hpp"
#include <optional>

namespace rtype::server {

  void InstanceLobbyScene::onEnter(rtk::EngineContext &context) {
    (void) context;

    std::cout << "[Room " << _dependency.instance.getRoomId() << "] Lobby started" << std::endl;
  }

  std::optional<ServerSceneType> InstanceLobbyScene::onUpdate(rtk::EngineContext &context, const ServerInput &input, float dt) {
    (void) context;
    (void) input;

    auto &instance = _dependency.instance;
    const auto nbPlayer = instance.getPlayerCount();
    if (nbPlayer == 0) {
      _lobbyTimer = 0.0f;
      return std::nullopt;
    }
    if (nbPlayer >= 2)
      return ServerSceneType::InGame;
    _lobbyTimer += dt;

    if (_lobbyTimer >= 30.0f)
      return ServerSceneType::InGame;
    return std::nullopt;
  }

  void InstanceLobbyScene::onExit(rtk::EngineContext& context) {
    (void)context;
    std::cout << "[Room " << _dependency.instance.getRoomId() << "] Leaving lobby" << std::endl;
  }


  void InstanceGameScene::onEnter(rtk::EngineContext &context) {
    (void) context;

    _dependency.instance.setGameActive(true);

    std::cout << "[Room " << _dependency.instance.getRoomId() << "] Game started" << std::endl;
  }

  std::optional<ServerSceneType> InstanceGameScene::onUpdate(rtk::EngineContext &context, const ServerInput &input, float dt) {
    (void) context;
    (void) input;
    (void) dt;

    auto &instance = _dependency.instance;
    const auto nbPlayer = instance.getPlayerCount();
    if (nbPlayer == 0) {
      return ServerSceneType::Lobby;
    }
    return std::nullopt;
  }

  void InstanceGameScene::onExit(rtk::EngineContext& context) {
    (void)context;

    _dependency.instance.setGameActive(false);
    std::cout << "[Room " << _dependency.instance.getRoomId() << "] Leaving game" << std::endl;
  }

} //namespace rtype::server
