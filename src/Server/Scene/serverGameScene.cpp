#include "serverGameScene.hpp"
#include "components/Transform.hpp"
#include "game.hpp"
#include "sceneManager/sceneConcept.hpp"
#include <iostream>

namespace rtype::server {

ServerGameScene::ServerGameScene(ServerDependency &dep) : _dependency(dep) {}

void ServerGameScene::onEnter(rtk::EngineContext &context) {
  std::cout << "[Server InGame] Entering InGame Scene! Creating Stage 1..."
            << std::endl;
  _dependency.game.setGameActive(true);
  _dependency.game.createBackground(context);
  for (const auto &[playerId, dummy] : _dependency.game.getPlayers()) {
    _dependency.game.sendExistingEntities(playerId, context);
  }
  std::cout << "[Server InGame] Stage 1 initialized and sent to all players."
            << std::endl;
}

std::optional<ServerSceneType>
ServerGameScene::onUpdate(rtk::EngineContext &context, const ServerInput &input,
                          float dt) {
  (void)input;
  (void)dt;

  _dependency.game.handleConnections(context);
  _dependency.game.handleInputs(context);
  _dependency.game.handleEntityQueries(context);
  _dependency.game.cleanupOffscreenEnvironment(context);
  _dependency.game.incrementServerTick();

  if (_dependency.game.getServerTick() % 3 == 0) {
    _dependency.game.sendSnapshots(context);
  }

  if (_dependency.game.getPlayerCount() == 0) {
    std::cout
        << "[Server InGame] All players disconnected. Returning to Lobby..."
        << std::endl;
    return ServerSceneType::Lobby;
  }

  bool isAnyPlayerAlive = false;
  auto &transforms = context.registry.get_components<component::Transform>();

  for (const auto &[connId, entity] : _dependency.game.getPlayers()) {
    if (transforms.get(entity) != nullptr) {
      isAnyPlayerAlive = true;
      break;
    }
  }

  if (!isAnyPlayerAlive) {
    std::cout << "[Server InGame] GAME OVER" << std::endl;
    return ServerSceneType::Lobby;
  }
  return std::nullopt;
}

void ServerGameScene::onExit(rtk::EngineContext &context) {
  std::cout << "[Server InGame] Leaving InGame Scene. Resetting game..."
            << std::endl;
  _dependency.game.setGameActive(false);

  auto view = context.registry.view<component::Transform>();

  for (auto entity : view) {
    context.registry.remove_entity_from_all_pools(entity);
    context.registry.kill_entity(entity);
  }

  context.registry.flush();
  _dependency.game.resetGameState();
}

} // namespace rtype::server