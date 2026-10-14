#pragma once

#include "sceneManager/sceneConcept.hpp"
#include "serverScene.hpp"
#include <optional>

namespace rtype::server {

class ServerGameScene {
public:
  static constexpr ServerSceneType Type = ServerSceneType::InGame;

  ServerGameScene(ServerDependency &dep);

  void onEnter(rtk::EngineContext &context);
  std::optional<ServerSceneType> onUpdate(rtk::EngineContext &context,
                                          const ServerInput &input, float dt);
  void onExit(rtk::EngineContext &context);

private:
  ServerDependency &_dependency;
};

} // namespace rtype::server