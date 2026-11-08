#pragma once

#include "EngineContext.hpp"
#include "GameInstance/Scene/InstanceScenes.hpp"
#include "Protocol/InputPayload.hpp"
#include "Server.hpp"
#include "collision/collisionBuffer.hpp"
#include "sceneManager/sceneManager.hpp"
#include "serverScene.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace rtype::server {

/**
 * @brief Owns an isolated ECS world and scene lifecycle for one room.
 *
 * Not thread safe: once started, it is only used by the thread of its
 * RoomWorker.
 */
class GameInstance {
public:
  /** @brief Sentinel used when a connected player has no live entity. */
  static constexpr std::size_t NO_ENTITY = static_cast<std::size_t>(-1);

  /**
   * @brief Constructs an isolated simulation for a room.
   * @param roomId Identifier of the room.
   * @param server Shared network server.
   * @param events Queue used to notify the main thread.
   */
  explicit GameInstance(std::uint32_t roomId, Server &server,
                        RoomEventQueue &events);

  /** @brief Instances cannot be copied. */
  GameInstance(const GameInstance &) = delete;
  /** @brief Copy assignment is disabled. */
  GameInstance &operator=(const GameInstance &) = delete;
  /** @brief Instances cannot be moved. */
  GameInstance(GameInstance &&) = delete;
  /** @brief Move assignment is disabled. */
  GameInstance &operator=(GameInstance &&) = delete;
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
  const std::unordered_map<std::uint32_t, std::size_t> &getPlayers() const;
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
   * @brief Asks the lobby scene to start a round at its next update.
   */
  void requestGameStart();
  /**
   * @brief Reads and clears the request made with requestGameStart().
   * @return True if a round must start.
   */
  bool consumeGameStartRequest();
  /**
   * @brief Accesses the isolated ECS engine context.
   * @return Reference to the instance context.
   */
  rtk::EngineContext &getContext();
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
  void addSystems(Server &server);
  /**
   * @brief Registers gameplay event handlers for this instance.
   * @param server Shared network server.
   */
  void subscribeEvents(Server &server);
  /**
   * @brief Applies a routed network input to this instance.
   * @param inputEvent Input received from a client.
   * @param server Shared network server.
   */
  void processInput(const Server::InputEvent &inputEvent, Server &server);
  /**
   * @brief Creates and tracks a player entity for a connection.
   * @param connectionId New connection identifier.
   * @param server Shared network server.
   * @return True if the player was created.
   */
  bool addConnectedPlayer(std::uint32_t connectionId, Server &server);
  /**
   * @brief Replicates current instance entities to one client.
   * @param connectionId Recipient connection identifier.
   * @param server Shared network server.
   */
  void sendExistingEntities(std::uint32_t connectionId, Server &server);
  /**
   * @brief Destroys a player entity and removes its connection entry.
   * @param connectionId Connection to remove.
   * @param server Shared network server.
   * @return True if the player was removed.
   */
  bool removeConnectedPlayer(std::uint32_t connectionId, Server &server);
  /**
   * @brief Sends this instance world snapshots to its players.
   * @param server Shared network server.
   */
  void sendSnapshots(Server &server);
  /**
   * @brief Answers an entity query using this instance registry.
   * @param queryEvent Entity lookup request.
   * @param server Shared network server.
   */
  void processEntityQuery(const Server::QueryEntityEvent &queryEvent,
                          Server &server);
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
  bool hasCompletedAllStages() const { return _allStagesCompleted; }

  /**
   * @brief Returns the identifier of the current stage.
   * @return Reference to the current stage identifier.
   */
  const std::string &getCurrentStageId() const { return _currentStageId; }

  /**
   * @brief Returns the elapsed time in the current stage.
   * @return Elapsed stage time in seconds.
   */
  float getStageTimer() const { return _stageTimer; }
  /**
   * @brief Ends the round and destroys its gameplay entities.
   * @param server Shared network server.
   */
  void endGame(Server &server);
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
  /**
   * @brief Actions held by a player, and when they started to be applied.
   *
   * It lets a snapshot tell the client which of its ticks the ship state
   * accounts for: the held actions were applied from the client tick
   * `clientTick` (server tick `serverTick`) and have been applied for every
   * server tick since, so the ship state is the one of the client tick
   * `clientTick + (now - serverTick)`.
   */
  struct InputRun {
    rtype::protocol::InputAction actions = rtype::protocol::InputAction::NoneAction;
    std::uint32_t clientTick = 0;
    std::uint32_t serverTick = 0;
  };

  /// Client tick the ship of a player is simulated up to, 0 if no input yet.
  std::uint32_t ackedInputTick(std::uint32_t connectionId) const;

  /**
   * @brief How often one client receives snapshots.
   *
   * A client whose link is saturated gets fewer snapshots, otherwise the queue
   * of the link grows and everything arrives late. The client is detected by
   * the round trip time: when its smallest RTT of the last half second is well
   * above the smallest ever seen (the RTT of an empty link), packets are
   * waiting in a queue.
   */
  struct SnapshotRate {
    std::uint32_t divisor = 1; ///< Send one snapshot out of `divisor`.
    float baseRtt = 0.0f;      ///< Smallest RTT seen, in ms, 0 if none yet.
    std::uint32_t cleanChecks = 0; ///< Checks in a row without congestion.
  };

  /**
   * @brief Tells whether an entity is stage scenery (a ground tile, a ramp, a
   * reactor, a gate).
   *
   * Scenery scrolls with a constant velocity that the client knows from the
   * blueprint of the entity, so it moves it by itself between two snapshots.
   * It is sent to a client when it first appears and then only now and then,
   * to correct any drift, which removes most of the entries of a snapshot.
   */
  bool isScenery(std::uint16_t archetypeId) const;

  /** @brief Server ticks after which scenery already sent is sent again. */
  static std::uint32_t sceneryRefreshTicks(std::uint32_t networkId);

  /** @brief Forgets the scenery sent for entities that no longer exist. */
  void pruneScenerySentTicks(std::size_t aliveEntities);

  /**
   * @brief Lowers or raises the snapshot rate of every client, four times per
   * second.
   */
  void updateSnapshotRates(Server &server);

  std::uint32_t _roomId;
  std::unordered_map<std::uint32_t, std::size_t> _players;
  std::unordered_map<std::uint32_t, InputRun> _inputRuns;
  std::unordered_map<std::uint32_t, SnapshotRate> _snapshotRates;
  std::uint32_t _snapshotCount = 0; ///< Snapshot periods since the start.
  std::uint32_t _serverTick = 0;
  bool _isActive = false;
  bool _gameStartRequested = false;
  bool _systemsStarted = false;
  bool _initialized = false;
  rtk::ecs::Registry _registry;
  rtk::ecs::Scheduler _scheduler;
  busEvent _eventBus;
  rtk::EngineContext _context{
      .registry = _registry, .scheduler = _scheduler, .eventBus = _eventBus};
  GameInstanceDependency _dependency;
  ServerInput _sceneInput;
  rtk::SceneManager<ServerSceneType, GameInstanceDependency, ServerInput,
                    InstanceLobbyScene, InstanceGameScene>
      _sceneManager;

  std::array<std::uint16_t, 4> _playerArchetypes{};
  std::array<std::uint16_t, 6> _sceneryArchetypes{};
  std::unordered_map<std::uint32_t,
                     std::unordered_map<std::uint32_t, std::uint32_t>>
      _scenerySentTicks;
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
  bool _stageRandomSpawns = true;

  /// Optional sinusoidal flight parameters of a scripted spawn.
  struct StageSpawnWave {
    float amplitude = 120.0f;
    float frequency = 2.5f;
  };

  /// Entity placed by the level editor, spawned once `time` is reached.
  struct StageSpawn {
    std::string blueprint;
    float time = 0.0f;
    float x = 1300.0f;
    float y = 0.0f;
    bool hasWave = false;
    StageSpawnWave wave;
  };

  std::vector<StageSpawn> _stageSpawns;
  std::size_t _nextStageSpawn = 0;

  /**
   * @brief Spawns every scripted entity whose time has been reached.
   * @param context Instance engine context.
   */
  void spawnDueStageEntities(rtk::EngineContext &context);

  bool _bossSpawned = false;
  std::uint32_t _bossNetId = 0; ///< Network id of the current stage boss.
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
  float spawnStageSegment(rtk::EngineContext &context,
                          const std::string &blueprint, float startX,
                          float spacing, bool symmetric);
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
  bool
  loadStageById(rtk::EngineContext &context, const std::string &stageId,
                const std::string &manifestPath = "assets/stages/stages.json");
  /**
   * @brief Loads the stage manifest and initializes progression.
   * @param context Engine context of the current game instance.
   * @param manifestPath Path to the stage manifest.
   */
  void loadStageManifest(
      rtk::EngineContext &context,
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
   * @brief Checks whether the boss of the current stage has been killed.
   * @param context Engine context of the current game instance.
   * @return True if the boss is dead or no longer exists.
   */
  bool isBossDefeated(rtk::EngineContext &context) const;
  /**
   * @brief Removes environment entities (and the boss room) from the stage.
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
