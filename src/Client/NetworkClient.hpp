#pragma once

#include <array>
#include <asio.hpp>
#include <asio/steady_timer.hpp>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include "Protocol/EntityPayload.hpp"
#include "Protocol/InputPayload.hpp"
#include "Protocol/MessageType.hpp"
#include "Protocol/SnapshotPayload.hpp"
#include "metrics/Metrics.hpp"
#include "protocol/MessageHeader.hpp"
#include "tracking/PacketTracker.hpp"

namespace rtype::client {
/**
 * @addtogroup api_client
 * @{
 */

/**
 * @brief Kind of event produced by the network client.
 */
enum class NetworkEventType {
  Connected,    ///< The server sent Welcome.
  Rejected,     ///< The server sent Rejected.
  Disconnected, ///< The connection was closed.
  Message       ///< A message that is not a replication message.
};

/**
 * @brief Connection event or raw message received from the server.
 */
struct NetworkEvent {
  NetworkEventType type = NetworkEventType::Message; ///< Event kind.
  std::uint32_t connectionId = 0; ///< Connection id given by the server.
  /// Message type, for Message events.
  rtype::protocol::MessageType messageType = protocol::MessageType::Invalid;
  /// Message payload, for Message events.
  std::vector<std::uint8_t> payload;
};

/// Decoded replication message.
using ReplicationPayload =
    std::variant<protocol::CreateEntityPayload, protocol::DestroyEntityPayload,
                 protocol::SnapshotPayload>;

/**
 * @brief Validated replication message received from the server.
 */
struct ReplicationEvent {
  std::uint32_t connectionId = 0; ///< Connection id given by the server.
  ReplicationPayload payload;     ///< Decoded message.
};

/**
 * @brief UDP client that performs the handshake, sends inputs and decodes
 * server messages.
 *
 * I/O runs on a dedicated thread, and received data is handed to the game
 * loop through thread safe queues read with pollEvent() and
 * pollReplicationEvent().
 */
class NetworkClient {
private:
  asio::io_context _ioContext;
  asio::ip::udp::socket _socket;
  asio::ip::udp::endpoint _serverEndpoint;
  asio::ip::udp::endpoint _receiveEndpoint;
  asio::steady_timer _flushTimer;
  std::thread _networkThread;
  PacketTracker _tracker;
  std::array<std::uint8_t, 2048> _receiveBuffer{};
  std::atomic_bool _running{false};
  std::atomic_bool _connected{false};
  std::atomic<std::uint32_t> _connectionId{0};
  std::uint32_t _nextInputSequence = 1;
  const std::chrono::steady_clock::time_point _inputEpoch =
      std::chrono::steady_clock::now();
  // State of the Input sending policy, used on the network thread only.
  bool _hasSentInput = false;
  rtype::protocol::InputAction _lastSentActions =
      rtype::protocol::InputAction::NoneAction;
  std::chrono::steady_clock::time_point _lastInputSent{};
  std::uint32_t _redundantInputsLeft = 0;
  std::chrono::steady_clock::time_point _lastKeepalive{};

  std::mutex _eventMutex;
  std::queue<NetworkEvent> _events;
  std::queue<ReplicationEvent> _replicationEvents;
  NetworkMetrics _metrics{};
  mutable std::mutex _metricsMutex;
  NetworkMetrics _metricsSnapshot{};

  void startReceive();
  void handleDatagram(std::size_t size);
  void handleHandshake(const std::uint8_t *data, std::size_t size);
  void handleConnectedPackets(const std::uint8_t *data, std::size_t size);
  void handleReplicationMessage(std::uint32_t connectionId,
                                protocol::MessageType type,
                                const std::vector<std::uint8_t> &payload);
  void doSend(std::vector<std::uint8_t> data);
  void enqueue(std::vector<std::uint8_t> payload, PacketPriority priority);
  void flush();
  void startFlushTimer();
  void sendKeepalive();
  void pushEvent(NetworkEvent event);
  void sendRoomCommand(protocol::MessageType type,
                       const std::vector<std::uint8_t> &payload);

public:
  /**
   * @brief Creates a client with a closed socket.
   */
  NetworkClient();

  /**
   * @brief Stops the client and joins the network thread.
   */
  ~NetworkClient();
  NetworkClient(const NetworkClient &) = delete;
  NetworkClient &operator=(const NetworkClient &) = delete;

  /**
   * @brief Resolves the server address, opens the socket and starts the
   * network thread.
   * @param host Server host name or IPv4 address.
   * @param port Server UDP port.
   * @return True on success, false if already running or resolution failed.
   */
  bool connect(const std::string &host, const std::string &port);

  /**
   * @brief Sends a Hello handshake if not connected yet.
   */
  void sendHello();

  /**
   * @brief Declares the actions held during the current frame.
   *
   * Call it every frame. An Input message is not sent for every call: the
   * actions are sent when they change (and `INPUT_REDUNDANT_COPIES` more times
   * against packet loss), then again every `INPUT_HEARTBEAT` while they stay
   * the same. The server keeps applying the last actions it received, so this
   * costs far less bandwidth than one message per frame.
   *
   * @param clientTick Current input tick, see inputTick().
   * @param actions Actions currently held.
   */
  void sendInput(std::uint32_t clientTick,
                 rtype::protocol::InputAction actions);

  /**
   * @brief Returns the current input tick: time elapsed since the creation of
   * the client, in 1 / INPUT_TICK_RATE of a second.
   *
   * The server uses the difference between ticks as a duration, so the tick
   * follows the clock and not the number of rendered frames.
   */
  std::uint32_t inputTick() const;

  /**
   * @brief Asks the server to describe an unknown entity.
   * @param networkId Network id of the entity.
   */
  void sendQueryEntity(std::uint32_t networkId);

  /**
   * @brief Sends raw bytes to the server.
   * @param data Datagram, ignored if empty or larger than MAX_PAYLOAD_SIZE.
   */
  void send(const std::vector<std::uint8_t> &data);

  /**
   * @brief Pops the oldest connection event or raw message.
   * @param event Receives the event.
   * @return True if an event was popped.
   */
  bool pollEvent(NetworkEvent &event);

  /**
   * @brief Pops the oldest replication message.
   * @param event Receives the event.
   * @return True if an event was popped.
   */
  bool pollReplicationEvent(ReplicationEvent &event);

  /**
   * @brief Tells whether the server accepted the handshake.
   * @return True once Welcome was received.
   */
  bool isConnected() const;

  /**
   * @brief Tells whether the network thread is running.
   * @return True between connect() and stop().
   */
  bool isRunning() const;

  /**
   * @brief Returns the id given by the server.
   * @return The connection id, or 0 if not connected.
   */
  std::uint32_t getConnectionId() const;

  /**
   * @brief Closes the socket and joins the network thread.
   */
  void stop();

  /**
   * @brief Returns the client's current network metrics.
   * @return Thread-safe snapshot of network statistics.
   */
  NetworkMetrics getMetrics() const;

  /**
   * @brief List all the room instancied.
   */
  void sendListRoom();

  /**
   * @brief Create a room.
   * @param name The name of the future room.
   */
  void sendCreateRoom(const std::string &name);

  /**
   * @brief Join a selected room.
   * @param roomId the id of the room to join.
   */
  void sendJoinRoom(std::uint32_t roomId);

  /**
   * @brief Leave the room.
   */
  void sendLeaveRoom();

  /**
   * @brief Set the status of the player.
   * @param ready the status of the player.
   */
  void sendSetReady(bool ready);
};

/** @} */
} // namespace rtype::client
