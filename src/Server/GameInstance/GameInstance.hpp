#pragma once

#include "EngineContext.hpp"
#include "GameInstance/Scene/InstanceScenes.hpp"
#include "Server.hpp"
#include "collision/collisionBuffer.hpp"
#include "sceneManager/sceneManager.hpp"
#include "serverScene.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace rtype::server {

  /**
   * @brief Owns an isolated ECS world and scene lifecycle for one room.
   */
  class GameInstance {
    public:
      /** @brief Sentinel used when a connected player has no live entity. */
      static constexpr std::size_t NO_ENTITY =
          static_cast<std::size_t>(-1);

      /**
       * @brief Constructs the simulation for a room.
       * @param roomId Identifier of the owning room.
       * @param server Shared network server.
       */
      explicit GameInstance(std::uint32_t roomId, Server &server);

      /** @brief Instances cannot be copied. */
      GameInstance(const GameInstance&) = delete;
      /** @brief Copy assignment is disabled. */
      GameInstance& operator=(const GameInstance&) = delete;
      /** @brief Instances cannot be moved. */
      GameInstance(GameInstance&&) = delete;
      /** @brief Move assignment is disabled. */
      GameInstance& operator=(GameInstance&&) = delete;
      /**
       * @brief Returns the owning room identifier.
       * @return Room identifier.
       */
      std::uint32_t getRoomId() const;
      /**
       * @brief Stores a connection-to-entity association.
       * @param connectionId Connection identifier.
       * @param entity ECS entity identifier.
       */
      void addPlayer(std::uint32_t connectionId, std::size_t entity);
      /**
       * @brief Removes a connection-to-entity association.
       * @param connectionId Connection identifier.
       * @return True if a player entry was removed.
       */
      bool removePlayer(std::uint32_t connectionId);
      /**
       * @brief Checks whether the connection is tracked by this instance.
       * @param connectionId Connection identifier.
       * @return True if the connection is tracked.
       */
      bool hasPlayer(std::uint32_t connectionId) const;
      /**
       * @brief Returns the number of tracked connections.
       * @return Number of player entries.
       */
      std::size_t getPlayerCount() const;
      /**
       * @brief Returns tracked player connections and entity IDs.
       * @return Read-only connection-to-entity map.
       */
      const std::unordered_map<std::uint32_t, std::size_t>&
      getPlayers() const;
      /**
       * @brief Advances the instance simulation tick.
       */
      void incrementServerTick();
      /**
       * @brief Returns the current instance tick.
       * @return Tick counter.
       */
      std::uint32_t getServerTick() const;
      /**
       * @brief Checks whether the instance is in an active round.
       * @return True if the round is active.
       */
      bool isActive() const;
      /**
       * @brief Sets the round activity flag.
       * @param active True to mark the round active.
       */
      void setGameActive(bool active);
      /**
       * @brief Resets the per-round state of the instance.
       */
      void resetGameState();
      /**
       * @brief Accesses the isolated ECS engine context.
       * @return Reference to the instance context.
       */
      rtk::EngineContext& getContext();
      /**
       * @brief Initializes and starts the instance systems and scenes.
       * @param server Shared network server.
       */
      void start(Server &server);
      /**
       * @brief Updates the instance scene, ECS systems, and registry.
       * @param dt Elapsed time in seconds.
       */
      void update(float dt);
      /**
       * @brief Stops the instance scene and ECS systems.
       */
      void stop();
      /**
       * @brief Registers ECS components, factories, and blueprints.
       */
      void initialize();
      /**
       * @brief Returns the available player blueprint identifiers.
       * @return Read-only player archetype array.
       */
      const std::array<std::uint16_t, 4> &getPlayerArchetypes() const;
      /**
       * @brief Registers server-side ECS systems for this instance.
       * @param server Shared network server.
       */
      void addSystems(Server& server);
      /**
       * @brief Registers gameplay event handlers for this instance.
       * @param server Shared network server.
       */
      void subscribeEvents(Server& server);
      /**
       * @brief Applies a routed network input to this instance.
       * @param inputEvent Input received from a client.
       * @param server Shared network server.
       */
      void processInput(const Server::InputEvent& inputEvent, Server& server);
      /**
       * @brief Creates and tracks a player entity for a connection.
       * @param connectionId New connection identifier.
       * @param server Shared network server.
       * @return True if the player was created.
       */
      bool addConnectedPlayer(std::uint32_t connectionId, Server& server);
      /**
       * @brief Replicates current instance entities to one client.
       * @param connectionId Recipient connection identifier.
       * @param server Shared network server.
       */
      void sendExistingEntities(std::uint32_t connectionId, Server& server);
      /**
       * @brief Destroys a player entity and removes its connection entry.
       * @param connectionId Connection to remove.
       * @param server Shared network server.
       * @return True if the player was removed.
       */
      bool removeConnectedPlayer(std::uint32_t connectionId, Server& server);
      /**
       * @brief Sends this instance world snapshots to its players.
       * @param server Shared network server.
       */
      void sendSnapshots(Server& server);
      /**
       * @brief Answers an entity query using this instance registry.
       * @param queryEvent Entity lookup request.
       * @param server Shared network server.
       */
      void processEntityQuery(const Server::QueryEntityEvent &queryEvent, Server &server);
      /**
       * @brief Creates the environment entities for a new round.
       */
      void createBackground();
      /**
       * @brief Updates the current stage progression.
       * @param dt Elapsed time since the previous update, in seconds.
       */
      void updateStage(float dt);

      /**
       * @brief Checks whether every stage has been completed.
       * @return True if all stages are completed, false otherwise.
       */
      bool hasCompletedAllStages() const
      {
          return _allStagesCompleted;
      }

      /**
       * @brief Returns the identifier of the current stage.
       * @return Reference to the current stage identifier.
       */
      const std::string &getCurrentStageId() const
      {
          return _currentStageId;
      }

      /**
       * @brief Returns the elapsed time in the current stage.
       * @return Elapsed stage time in seconds.
       */
      float getStageTimer() const
      {
          return _stageTimer;
      }
      /**
       * @brief Ends the round and destroys its gameplay entities.
       * @param server Shared network server.
       */
      void endGame(Server& server);
      /**
       * @brief Recreates player entities for the next round.
       */
      void respawnPlayers();
      /**
       * @brief Checks whether any tracked player has a live entity.
       * @return True if at least one player entity is alive.
       */
      bool hasAlivePlayers();
    
    private:
      std::uint32_t _roomId;
      std::unordered_map<std::uint32_t, std::size_t> _players;
      std::uint32_t _serverTick = 0;
      bool _isActive = false;
      bool _systemsStarted = false;
      bool _initialized = false;
      rtk::ecs::Registry _registry;
      rtk::ecs::Scheduler _scheduler;
      busEvent _eventBus;
      rtk::EngineContext _context {
          .registry = _registry,
          .scheduler = _scheduler,
          .eventBus = _eventBus
      };
      GameInstanceDependency _dependency;
      ServerInput _sceneInput;
      rtk::SceneManager<
        ServerSceneType,
        GameInstanceDependency,
        ServerInput,
        InstanceLobbyScene,
        InstanceGameScene
      > _sceneManager;
      
      std::array<std::uint16_t, 4> _playerArchetypes{};
      rtk::collision::CollisionBuffer _collisionBuffer;
      bool _systemsRegistered = false;
      bool _eventsRegistered = false;
      std::string _currentStageId;
      std::string _nextStageId;
      std::string _stageName;
      float _stageTimer = 0.0f;
      float _stageDuration = 0.0f;
      std::size_t _stageObstaclesSpawned = 0;
      bool _stageActive = false;
      bool _allStagesCompleted = false;

      /**
       * @brief Registers components in the instance registry.
       * @param context Instance engine context.
       */
      void registerComponents(rtk::EngineContext &context);
      /**
       * @brief Registers entity factory builders.
       * @param context Instance engine context.
       */
      void registerBuilders(rtk::EngineContext &context);
      /**
       * @brief Loads entity blueprints and player archetypes.
       * @param context Instance engine context.
       */
      void loadEntityBlueprints(rtk::EngineContext &context);
      /**
       * @brief Creates an environment segment from a stage blueprint.
       * @param context Engine context of the current game instance.
       * @param blueprint Entity blueprint identifier.
       * @param startX Horizontal starting position.
       * @param spacing Horizontal spacing after the segment.
       * @param symmetric Whether to create a symmetrical segment.
       * @return Horizontal position following the created segment.
       */
      float spawnStageSegment(rtk::EngineContext &context, const std::string &blueprint,
          float startX, float spacing, bool symmetric);
      /**
       * @brief Loads a stage from a JSON file.
       * @param context Engine context of the current game instance.
       * @param stagePath Path to the stage JSON file.
       */
      void loadStage(rtk::EngineContext &context, const std::string &stagePath);
      /**
       * @brief Loads a stage identified in the stage manifest.
       * @param context Engine context of the current game instance.
       * @param stageId Identifier of the stage to load.
       * @param manifestPath Path to the stage manifest.
       * @return True if the stage was loaded successfully.
       */
      bool loadStageById(rtk::EngineContext &context,
          const std::string &stageId,
          const std::string &manifestPath = "assets/stages/stages.json");
      /**
       * @brief Loads the stage manifest and initializes progression.
       * @param context Engine context of the current game instance.
       * @param manifestPath Path to the stage manifest.
       */
      void loadStageManifest(rtk::EngineContext &context,
          const std::string &manifestPath = "assets/stages/stages.json");
      /**
       * @brief Checks whether the current stage is finished.
       * @param context Engine context of the current game instance.
       * @return True if the current stage is finished.
       */
      bool isStageFinished(rtk::EngineContext &context) const;
      /**
       * @brief Advances to the next available stage.
       * @param context Engine context of the current game instance.
       * @return True if another stage was successfully loaded.
       */
      bool advanceToNextStage(rtk::EngineContext &context);
      /**
       * @brief Checks for remaining environment entities.
       * @param context Engine context of the current game instance.
       * @return True if environment entities are still present.
       */
      bool hasActiveEnvironmentEntities(rtk::EngineContext &context) const;
      /**
       * @brief Removes environment entities from the current stage.
       * @param context Engine context of the current game instance.
       */
      void clearEnvironmentEntities(rtk::EngineContext &context);
      /**
       * @brief Removes enemy entities from the current stage.
       * @param context Engine context of the current game instance.
       */
      void clearEnemies(rtk::EngineContext &context);
      /**
       * @brief Creates a gate environment segment.
       * @param startX Starting horizontal position.
       * @param symmetric Whether to create the mirrored segment.
       * @return Horizontal position after the segment.
       */
      float createGate(float startX, bool symmetric);
      /**
       * @brief Creates a ramp environment segment.
       * @param startX Starting horizontal position.
       * @param symmetric Whether to create the mirrored segment.
       * @return Horizontal position after the segment.
       */
      float createRamp(float startX, bool symmetric);
      /**
       * @brief Creates a reactor environment segment.
       * @param startX Starting horizontal position.
       * @param symmetric Whether to create the mirrored segment.
       * @return Horizontal position after the segment.
       */
      float createReactor(float startX, bool symmetric);
      /**
       * @brief Creates a high-ground environment segment.
       * @param startX Starting horizontal position.
       * @param symmetric Whether to create the mirrored segment.
       * @return Horizontal position after the segment.
       */
      float createHighGround(float startX, bool symmetric);
      /**
       * @brief Creates a low-ground environment segment.
       * @param startX Starting horizontal position.
       * @param symmetric Whether to create the mirrored segment.
       * @return Horizontal position after the segment.
       */
      float createLowGround(float startX, bool symmetric);
      /**
       * @brief Creates a medium-ground environment segment.
       * @param startX Starting horizontal position.
       * @param symmetric Whether to create the mirrored segment.
       * @return Horizontal position after the segment.
       */
      float createMediumGround(float startX, bool symmetric);

  };
} // namespace rtype::server

