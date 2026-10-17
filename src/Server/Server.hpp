#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <vector>

#include "Protocol/EntityPayload.hpp"
#include "Protocol/InputPayload.hpp"
#include "Protocol/MessageType.hpp"
#include "Protocol/SnapshotPayload.hpp"
#include "protocol/MessageHeader.hpp"
#include "server/NetworkServer.hpp"

namespace rtype::server {
/**
 * @addtogroup api_server
 * @{
 */

/**
 * @brief Game level layer over NetworkServer that decodes client messages
 * and encodes server messages.
 *
 * Decoded messages are pushed by the network thread into mutex protected
 * queues that the game loop drains with the poll methods.
 */
class Server {
public:
  /**
   * @brief Creates the server on a UDP port, without starting it.
   * @param port Local port to listen on.
   */
  explicit Server(std::uint16_t port);

  /**
   * @brief Stops the network thread.
   */
  ~Server();

  /**
   * @brief Input message received from a client.
   */
  struct InputEvent {
    std::uint32_t connectionId;   ///< Sender connection.
    protocol::InputPayload input; ///< Decoded payload.
  };

  /**
   * @brief Client connection or disconnection.
   */
  struct ConnectionEvent {
    /**
     * @brief Kind of connection change.
     */
    enum class Type {
      Connected,   ///< Handshake completed.
      Disconnected ///< Client sent Disconnect.
    };

    Type type;                  ///< Kind of change.
    std::uint32_t connectionId; ///< Affected connection.
  };

  /**
   * @brief QueryEntity message received from a client.
   */
  struct QueryEntityEvent {
    std::uint32_t connectionId; ///< Sender connection.
    std::uint32_t networkId;    ///< Entity the client asks about.
  };

  /**
   * @brief Starts the network thread.
   */
  void start();

  /**
   * @brief Stops the network thread.
   */
  void stop();

  /**
   * @brief Sends already encoded messages to a client.
   * @param connectionId Destination connection.
   * @param payload Messages, headers included.
   * @param priority Delivery guarantee of the packet.
   */
  void send(std::uint32_t connectionId,
            const std::vector<std::uint8_t> &payload,
            PacketPriority priority = PacketPriority::Unreliable);

  /**
   * @brief Pops the oldest Input message.
   * @param event Receives the message.
   * @return True if a message was popped.
   */
  bool pollInput(InputEvent &event);

  /**
   * @brief Pops the oldest connection change.
   * @param event Receives the change.
   * @return True if a change was popped.
   */
  bool pollConnectionEvent(ConnectionEvent &event);

  /**
   * @brief Pops the oldest QueryEntity message.
   * @param event Receives the message.
   * @return True if a message was popped.
   */
  bool pollQueryEntity(QueryEntityEvent &event);

  /**
   * @brief Sends a CreateEntity message.
   * @param connectionId Destination connection.
   * @param payload Message payload.
   * @return False if the payload is invalid or cannot be encoded.
   */
  bool sendCreateEntity(std::uint32_t connectionId,
                        const protocol::CreateEntityPayload &payload);

  /**
   * @brief Sends a DestroyEntity message.
   * @param connectionId Destination connection.
   * @param payload Message payload.
   * @return False if the payload is invalid or cannot be encoded.
   */
  bool sendDestroyEntity(std::uint32_t connectionId,
                         const protocol::DestroyEntityPayload &payload);

  /**
   * @brief Sends a Snapshot message.
   * @param connectionId Destination connection.
   * @param payload Message payload.
   * @return False if the snapshot has too many states or cannot be encoded.
   */
  bool sendSnapshot(std::uint32_t connectionId,
                    const protocol::SnapshotPayload &payload);

private:
  void handleMessage(std::uint32_t connectionId, MessageId type,
                     const std::vector<std::uint8_t> &payload);
  bool sendMessage(std::uint32_t connectionId, protocol::MessageType type,
                   const std::vector<std::uint8_t> &payload);

  NetworkServer _network;
  std::mutex _inputMutex;
  std::queue<InputEvent> _inputQueue;
  std::unordered_map<std::uint32_t, std::uint32_t> _lastInputSequence;

  std::mutex _connectionMutex;
  std::queue<ConnectionEvent> _connectionQueue;

  std::mutex _queryMutex;
  std::queue<QueryEntityEvent> _queryQueue;

  std::function<void(std::uint32_t connectionId)> _onClientConnected;
  std::function<void(std::uint32_t connectionId)> _onClientDisconnected;
};

/** @} */
} // namespace rtype::server
