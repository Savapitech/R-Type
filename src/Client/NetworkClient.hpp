#pragma once

#include <array>
#include <asio.hpp>
#include <asio/steady_timer.hpp>
#include <atomic>
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
  std::mutex _eventMutex;
  std::queue<NetworkEvent> _events;
  std::queue<ReplicationEvent> _replicationEvents;

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
  void pushEvent(NetworkEvent event);

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
   * @brief Sends an Input message with the next input sequence number.
   * @param clientTick Current client frame counter.
   * @param actions Actions currently held.
   */
  void sendInput(std::uint32_t clientTick,
                 rtype::protocol::InputAction actions);

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
};

/** @} */
} // namespace rtype::client
