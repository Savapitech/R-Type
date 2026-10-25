#pragma once

#include "EngineContext.hpp"
#include "Scene/serverGameScene.hpp"
#include "Scene/serverLobbyScene.hpp"
#include "Server.hpp"
#include "collision/collisionBuffer.hpp"
#include "factoryDataDriven/factoryDataDriven.hpp"
#include "sceneManager/sceneManager.hpp"
#include "serverScene.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>

namespace rtype::server {
/**
 * @addtogroup api_server
 * @{
 */

/**
 * @brief Authoritative game simulation run by the server.
 *
 * Owns the network server, the lobby and in game scenes, and the player
 * entities, and replicates the world to every connected client.
 */
class Game {
  friend class ServerLobbyScene;
  friend class ServerGameScene;

public:
  /// Entity of a player whose ship was destroyed.
  static constexpr std::size_t NO_ENTITY =
      std::numeric_limits<std::size_t>::max();

  /**
   * @brief Creates the game and its network server.
   * @param port UDP port to listen on.
   */
  explicit Game(std::uint16_t port = 4242);
  ~Game();

  /**
   * @brief Registers components, loads blueprints, adds the systems and
   * starts the network server.
   * @param context Engine services.
   */
  void onLoad(rtk::EngineContext &context);

  /**
   * @brief Updates the active scene.
   * @param context Engine services.
   * @param dt Elapsed time since the last frame, in seconds.
   */
  void onUpdate(rtk::EngineContext &context, float dt);

  /**
   * @brief Exits the scene and stops the network server.
   * @param context Engine services.
   */
  void onUnload(rtk::EngineContext &context);

  /**
   * @brief Tells the engine whether to keep running.
   * @return True while the server runs.
   */
  bool shouldContinue() const;

  /**
   * @brief Advances the server tick by one.
   */
  void incrementServerTick() { _serverTick++; }

  /**
   * @brief Returns the server tick.
   * @return Number of simulated frames since the game started.
   */
  std::uint32_t getServerTick() const { return _serverTick; }

  /**
   * @brief Returns the number of connected players.
   * @return The player count.
   */
  std::size_t getPlayerCount() const { return _players.size(); }

  /**
   * @brief Returns the connected players.
   * @return Map from connection id to player entity.
   */
  const std::unordered_map<std::uint32_t, std::size_t> &getPlayers() const {
    return _players;
  }

  /**
   * @brief Tells whether a round is being played.
   * @return True in the in game scene.
   */
  bool isGameActive() const { return _isGameActive; }

  /**
   * @brief Marks a round as started or stopped.
   * @param active True while a round is played.
   */
  void setGameActive(bool active) { _isGameActive = active; }

  /**
   * @brief Forgets every player and resets the tick and round state.
   */
  void resetGameState() {
    _players.clear();
    _serverTick = 0;
    _isGameActive = false;
    _currentStageId.clear();
    _nextStageId.clear();
    _stageName.clear();
    _stageTimer = 0.0f;
    _stageDuration = 0.0f;
    _stageObstaclesSpawned = 0;
    _stageActive = false;
    _allStagesCompleted = false;
  }

  /**
   * @brief Sends DestroyEntity with reason GameEnded for every replicated
   * entity to every player.
   * @param context Engine services.
   */
  void broadcastGameEnded(rtk::EngineContext &context);

  /**
   * @brief Loads a stage defined in a JSON file.
   * @param context Engine services.
   * @param stagePath Path to the stage JSON file.
   */
  void loadStage(rtk::EngineContext &context, const std::string &stagePath);

  /**
   * @brief Loads a stage by its identifier from the manifest.
   * @param context Engine services.
   * @param stageId Stage identifier in the manifest.
   * @param manifestPath Path to the stages manifest JSON file.
   * @return True if the stage was successfully found and loaded.
   */
  bool
  loadStageById(rtk::EngineContext &context, const std::string &stageId,
                const std::string &manifestPath = "assets/stages/stages.json");

  /**
   * @brief Loads the default stage from the stages manifest file.
   * @param context Engine services.
   * @param manifestPath Path to the stages manifest JSON file.
   */
  void loadStageManifest(
      rtk::EngineContext &context,
      const std::string &manifestPath = "assets/stages/stages.json");

  /**
   * @brief Updates stage progression timer and checks stage completion.
   * @param context Engine services.
   * @param dt Elapsed time since the last frame, in seconds.
   */
  void updateStage(rtk::EngineContext &context, float dt);

  /**
   * @brief Checks if the current stage has met its completion condition.
   * @param context Engine services.
   * @return True if the stage has ended.
   */
  bool isStageFinished(rtk::EngineContext &context) const;

  /**
   * @brief Transitions to the next stage if available.
   * @param context Engine services.
   * @return True if a next stage was found and loaded, false if no more stages.
   */
  bool advanceToNextStage(rtk::EngineContext &context);

  /**
   * @brief Returns whether all configured stages have been completed.
   * @return True if the entire game sequence has finished.
   */
  bool hasCompletedAllStages() const { return _allStagesCompleted; }

  /**
   * @brief Returns the identifier of the current active stage.
   * @return Current stage id.
   */
  const std::string &getCurrentStageId() const { return _currentStageId; }

  /**
   * @brief Returns the current stage progression timer in seconds.
   * @return Elapsed time in current stage.
   */
  float getStageTimer() const { return _stageTimer; }

private:
  void registerComponents(rtk::EngineContext &context);
  void registerBuilders(rtk::EngineContext &context);
  void loadEntityBlueprints(rtk::EngineContext &context);
  void subscribeEvents(rtk::EngineContext &context);
  void addSystems(rtk::EngineContext &context);

  void handleConnections(rtk::EngineContext &context);
  void handleInputs(rtk::EngineContext &context);
  void handleEntityQueries(rtk::EngineContext &context);
  void cleanupOffscreenEnvironment(rtk::EngineContext &context);
  void sendSnapshots(rtk::EngineContext &context);
  void createBackground(rtk::EngineContext &context);

  bool hasActiveEnvironmentEntities(rtk::EngineContext &context) const;
  void clearEnvironmentEntities(rtk::EngineContext &context);
  void clearEnemies(rtk::EngineContext &context);

  float spawnStageSegment(rtk::EngineContext &context,
                          const std::string &blueprint, float startX,
                          float spacing, bool symmetric);

  void sendExistingEntities(std::uint32_t connectionId,
                            rtk::EngineContext &context);
  void broadcastPlayerCreated(std::uint32_t connectionId,
                              rtk::EngineContext &context);
  void broadcastPlayerDestroyed(std::uint32_t connectionId,
                                std::uint32_t networkId);

  ServerDependency _serverContext;
  ServerInput _serverInput;

  rtk::SceneManager<ServerSceneType, ServerDependency, ServerInput,
                    ServerLobbyScene, ServerGameScene>
      _sceneManager;

  Server _server;
  std::unordered_map<std::uint32_t, std::size_t> _players;
  std::array<std::uint16_t, 4> _playerArchetypes{};
  std::uint32_t _serverTick = 0;
  bool _running = true;
  bool _isGameActive = false;
  rtk::collision::CollisionBuffer _collisionBuffer;

  std::string _currentStageId;
  std::string _nextStageId;
  std::string _stageName;
  float _stageTimer = 0.0f;
  float _stageDuration = 0.0f;
  std::size_t _stageObstaclesSpawned = 0;
  bool _stageActive = false;
  bool _allStagesCompleted = false;
};

/** @} */
} // namespace rtype::server
