#pragma once

#include "serverScene.hpp"
#include "EngineContext.hpp"
#include <optional>


namespace rtype::server {

  class InstanceLobbyScene {
    public:
      static constexpr ServerSceneType Type = ServerSceneType::Lobby;

      explicit InstanceLobbyScene(GameInstanceDependency &dependency)
        : _dependency(dependency) {}
      void onEnter(rtk::EngineContext &context);
      std::optional<ServerSceneType> onUpdate(rtk::EngineContext &context,
          const ServerInput &input, float dt);
      void onExit(rtk::EngineContext &context);

    private:
      GameInstanceDependency &_dependency;
      float _lobbyTimer = 0.0f;
  };

  class InstanceGameScene {
    public:
      static constexpr ServerSceneType Type = ServerSceneType::InGame;
      explicit InstanceGameScene(GameInstanceDependency &dependency)
        : _dependency(dependency) {}
      void onEnter(rtk::EngineContext &context);
      std::optional<ServerSceneType> onUpdate(rtk::EngineContext &context,
          const ServerInput &input, float dt);
      void onExit(rtk::EngineContext &context);

    private:
      GameInstanceDependency &_dependency;
  };
} //namespace rtype::server
