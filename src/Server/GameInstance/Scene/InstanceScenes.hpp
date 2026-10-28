#pragma once

#include "serverScene.hpp"
#include "EngineContext.hpp"

#include <optional>

namespace rtype::server {

  /**
   * @brief Lobby scene scoped to one game instance.
   */
  class InstanceLobbyScene {
    public:
      /** @brief Scene identifier used by SceneManager. */
      static constexpr ServerSceneType Type = ServerSceneType::Lobby;

      /**
       * @brief Creates a lobby scene.
       * @param dependency Game instance and shared network server.
       */
      explicit InstanceLobbyScene(GameInstanceDependency &dependency)
        : _dependency(dependency) {}

      /**
       * @brief Resets lobby state when entering the scene.
       * @param context Instance engine context.
       */
      void onEnter(rtk::EngineContext &context);

      /**
       * @brief Updates lobby countdown and determines the next scene.
       * @param context Instance engine context.
       * @param input Scene input data.
       * @param dt Elapsed time in seconds.
       * @return Next scene, or std::nullopt to remain in the lobby.
       */
      std::optional<ServerSceneType> onUpdate(
          rtk::EngineContext &context,
          const ServerInput &input,
          float dt);

      /**
       * @brief Handles departure from the lobby scene.
       * @param context Instance engine context.
       */
      void onExit(rtk::EngineContext &context);

    private:
      GameInstanceDependency &_dependency;
  };

  /**
   * @brief Running-game scene scoped to one game instance.
   */
  class InstanceGameScene {
    public:
      /** @brief Scene identifier used by SceneManager. */
      static constexpr ServerSceneType Type = ServerSceneType::InGame;

      /**
       * @brief Creates an in-game scene.
       * @param dependency Game instance and shared network server.
       */
      explicit InstanceGameScene(GameInstanceDependency &dependency)
        : _dependency(dependency) {}

      /**
       * @brief Starts the round and initializes its entities.
       * @param context Instance engine context.
       */
      void onEnter(rtk::EngineContext &context);

      /**
       * @brief Updates round transitions for this game instance.
       * @param context Instance engine context.
       * @param input Scene input data.
       * @param dt Elapsed time in seconds.
       * @return Next scene, or std::nullopt to continue playing.
       */
      std::optional<ServerSceneType> onUpdate(
          rtk::EngineContext &context,
          const ServerInput &input,
          float dt);

      /**
       * @brief Ends the round and cleans up its entities.
       * @param context Instance engine context.
       */
      void onExit(rtk::EngineContext &context);

    private:
      GameInstanceDependency &_dependency;
  };

} // namespace rtype::server
