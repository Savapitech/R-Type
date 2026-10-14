#pragma once

#include "sceneManager/sceneConcept.hpp"
#include "serverScene.hpp"
#include <optional>

namespace rtype::server {

class ServerLobbyScene {
public:
  static constexpr ServerSceneType Type = ServerSceneType::Lobby;

  ServerLobbyScene(ServerDependency &dep);

  void onEnter(rtk::EngineContext &context);
  std::optional<ServerSceneType> onUpdate(rtk::EngineContext &context,
                                          const ServerInput &input, float dt);
  void onExit(rtk::EngineContext &context);

private:
  ServerDependency &_dependency;
  float _lobbyTimer = 0.0f;
  int _lastPrintedCountdown = -1;
};

} // namespace rtype::server