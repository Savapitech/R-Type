#pragma once

#include "EngineContext.hpp"
#include "GameInstance/RoomChannels.hpp"
#include "GameInstance/RoomWorker.hpp"
#include "Room/RoomManager.hpp"
#include "Scene/serverGameScene.hpp"
#include "Scene/serverLobbyScene.hpp"
#include "Server.hpp"
#include "collision/collisionBuffer.hpp"
#include "factoryDataDriven/factoryDataDriven.hpp"
#include "sceneManager/sceneManager.hpp"
#include "serverScene.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
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
  }

  /**
   * @brief Sends DestroyEntity with reason GameEnded for every replicated
   * entity to every player.
   * @param context Engine services.
   */
  void broadcastGameEnded(rtk::EngineContext &context);

private:
  void registerComponents(rtk::EngineContext &context);
  void registerBuilders(rtk::EngineContext &context);
  void loadEntityBlueprints(rtk::EngineContext &context);
  void subscribeEvents(rtk::EngineContext &context);
  void addSystems(rtk::EngineContext &context);

  void handleConnections(rtk::EngineContext &context);
  void handleRoomRequests();
  /**
   * @brief Applies the notifications sent by the room threads.
   */
  void handleRoomEvents();
  void broadcastRoomState(std::uint32_t roomId);
  /**
   * @brief Broadcasts a chat message to every player of the sender's room.
   * @param connectionId Sender connection.
   * @param text Validated message content.
   */
  void broadcastChatMessage(std::uint32_t connectionId,
                            const std::string &text);
  /**
   * @brief Removes a connection from its room.
   * @param connectionId Connection to remove.
   * @param notifyClient True to answer with RoomLeft once the room stopped
   * replicating to this connection.
   * @return True if the connection was in a room.
   */
  bool removePlayerFromRoom(std::uint32_t connectionId, bool notifyClient);
  /**
   * @brief Deletes the oldest empty rooms until a new room fits in the list.
   */
  void freeRoomListSlot();
  void cleanupOffscreenEnvironment(rtk::EngineContext &context);
  void sendSnapshots(rtk::EngineContext &context);

  void sendExistingEntities(std::uint32_t connectionId,
                            rtk::EngineContext &context);
  void broadcastPlayerCreated(std::uint32_t connectionId,
                              rtk::EngineContext &context);
  void broadcastPlayerDestroyed(std::uint32_t connectionId,
                                std::uint32_t networkId);
  /**
   * @brief Displays network statistics at one-second intervals.
   */
  void updateNetworkMetrics();

  /** @brief Request answered with RoomCreated or RoomJoined. */
  enum class JoinKind { Create, Join };

  /** @brief Join waiting for its room thread to create the player entity. */
  struct PendingJoin {
    std::uint32_t roomId;    ///< Room being joined.
    std::uint64_t requestId; ///< Identifies this join among older ones.
    JoinKind kind;           ///< Message answering the client.
  };

  /**
   * @brief Returns the worker of a room, starting it if needed.
   * @param roomId Room identifier.
   * @return Pointer to the worker, or nullptr on failure.
   */
  RoomWorker *createRoomWorker(std::uint32_t roomId);
  /**
   * @brief Looks up a worker by room identifier.
   * @param roomId Room identifier.
   * @return Pointer to the worker, or nullptr if missing.
   */
  RoomWorker *getRoomWorker(std::uint32_t roomId);
  /**
   * @brief Stops and removes a worker.
   * @param roomId Room identifier.
   * @return True if the worker was removed.
   */
  bool destroyRoomWorker(std::uint32_t roomId);
  /**
   * @brief Routes a connection to its room and asks for its player entity.
   * @param connectionId Connection joining the room.
   * @param roomId Room being joined, the connection is already a member.
   * @param worker Worker of the room.
   * @param kind Message that will answer the client.
   */
  void beginJoin(std::uint32_t connectionId, std::uint32_t roomId,
                 RoomWorker &worker, JoinKind kind);
  /**
   * @brief Answers a create or join request.
   * @param connectionId Destination connection.
   * @param kind Message to send.
   * @param result Outcome of the request.
   */
  void sendJoinResult(std::uint32_t connectionId, JoinKind kind,
                      const protocol::RoomResultPayload &result);
  /**
   * @brief Starts a round if every player of a waiting room is ready.
   * @param roomId Room identifier.
   */
  void startRoomIfReady(std::uint32_t roomId);
  /**
   * @brief Answers the client once the room processed its join.
   * @param event PlayerAdded notification.
   */
  void finishJoin(const RoomEvent &event);
  /**
   * @brief Puts a room back in the waiting state after a round.
   * @param roomId Room identifier.
   */
  void finishGame(std::uint32_t roomId);
  /**
   * @brief Sends the players of a crashed room back to the room list.
   * @param event Crashed notification.
   */
  void handleRoomCrash(const RoomEvent &event);

  // The network thread reads the router and room threads push events until
  // they are joined: both must be destroyed after _server and _roomWorkers.
  RoomRouter _roomRouter;
  RoomEventQueue _roomEvents;
  Server _server;
  std::unordered_map<std::uint32_t, std::size_t> _players;
  std::array<std::uint16_t, 4> _playerArchetypes{};
  std::uint32_t _serverTick = 0;
  bool _running = true;
  bool _isGameActive = false;
  rtk::collision::CollisionBuffer _collisionBuffer;
  RoomManager _roomManager;
  std::uint32_t _defaultRoomId = 0;
  std::chrono::steady_clock::time_point _lastMetricsTime{
      std::chrono::steady_clock::now()};
  NetworkMetrics _previousMetrics{};
  bool _metricsInitialized = false;
  std::unordered_map<std::uint32_t, NetworkMetrics> _previousClientMetrics;
  std::unordered_map<std::uint32_t, PendingJoin> _pendingJoins;
  /// Time of the last chat message accepted from each connection.
  std::unordered_map<std::uint32_t, std::chrono::steady_clock::time_point>
      _lastChatMessage;
  std::uint64_t _nextJoinRequestId = 1;
  // Declared after _server: room threads must stop before it is destroyed.
  std::unordered_map<std::uint32_t, std::unique_ptr<RoomWorker>> _roomWorkers;
};

/** @} */
} // namespace rtype::server
