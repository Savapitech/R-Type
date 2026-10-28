#include "serverLobbyScene.hpp"
#include "game.hpp"
#include "sceneManager/sceneConcept.hpp"
#include "serverScene.hpp"
#include <iostream>

namespace rtype::server {

ServerLobbyScene::ServerLobbyScene(ServerDependency &dep)
    : _dependency(dep), _lobbyTimer(0.0f), _lastPrintedCountdown(-1) {}

void ServerLobbyScene::onEnter(rtk::EngineContext &) {
  _lobbyTimer = 0.0f;
  _lastPrintedCountdown = -1;
  std::cout << "[Server Lobby] Waiting for players... (Game starts with >= 2 "
               "players or after 30s with >= 1 player)"
            << std::endl;
}

std::optional<ServerSceneType>
ServerLobbyScene::onUpdate(rtk::EngineContext &context,
                           const ServerInput &, float dt) {
  _dependency.game.handleConnections(context);
  _dependency.game.handleInputs(context);
  _dependency.game.handleEntityQueries(context);
  _dependency.game.incrementServerTick();

  if (_dependency.game.getServerTick() % 3 == 0) {
    _dependency.game.sendSnapshots(context);
  }

  std::size_t playerCount = _dependency.game.getPlayerCount();
  if (playerCount == 0) {
    _lobbyTimer = 0.0f;
    _lastPrintedCountdown = -1;
    return std::nullopt;
  }

  if (playerCount >= 2) {
    std::cout << "[Server Lobby] " << playerCount
              << " players connected! Launching InGame scene!" << std::endl;
    return ServerSceneType::InGame;
  }

  _lobbyTimer += dt;
  int remaining = 30 - static_cast<int>(_lobbyTimer);
  if (remaining >= 0 && remaining != _lastPrintedCountdown &&
      (remaining % 5 == 0 || remaining <= 5)) {
    _lastPrintedCountdown = remaining;
    std::cout << "[Server Lobby] " << playerCount
              << " player(s) waiting... Starting in " << remaining
              << "s (or when 2nd player connects)..." << std::endl;
  }

  if (_lobbyTimer >= 30.0f) {
    std::cout << "[Server Lobby] 30 seconds timer expired! Starting InGame "
                 "scene with "
              << playerCount << " player(s)!" << std::endl;
    return ServerSceneType::InGame;
  }

  return std::nullopt;
}

void ServerLobbyScene::onExit(rtk::EngineContext &) {
  std::cout << "[Server Lobby] Lobby closed, transitioning to InGame."
            << std::endl;
}

} // namespace rtype::server
