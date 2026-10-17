#pragma once

/**
 * @addtogroup api_network
 * @{
 */

#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../connection/Connection.hpp"
#include "../metrics/Metrics.hpp"
#include "../protocol/MessageParser.hpp"
#include "../transport/UdpTransport.hpp"

/**
 * @brief UDP server that runs the handshake, tracks connections and
 * dispatches messages.
 *
 * Network I/O runs on a dedicated thread, so every callback is invoked from
 * that thread and the caller must synchronize any data it shares with the
 * game loop.
 */
class NetworkServer {
public:
  /// Callback receiving the id of a connection.
  using ConnectionCallback = std::function<void(std::uint32_t connectionId)>;
  /// Callback receiving a message that the network layer does not handle.
  using MessageCallback =
      std::function<void(std::uint32_t connectionId, MessageId type,
                         const std::vector<std::uint8_t> &payload)>;

  /**
   * @brief Creates a server bound to a UDP port, without starting it.
   * @param port Local port to listen on.
   */
  NetworkServer(std::uint16_t port);

  /**
   * @brief Stops the server and joins the network thread.
   */
  ~NetworkServer();

  /**
   * @brief Starts receiving datagrams on the network thread.
   */
  void start();

  /**
   * @brief Closes the socket and joins the network thread.
   */
  void stop();

  /**
   * @brief Sends messages to a client inside a new connected packet.
   * @param connectionId Destination connection.
   * @param payload Encoded messages, headers included.
   * @param priority Delivery guarantee, Critical packets are sent first and
   * resent until acknowledged.
   */
  void send(std::uint32_t connectionId,
            const std::vector<std::uint8_t> &payload,
            PacketPriority priority = PacketPriority::Unreliable);

  /**
   * @brief Sets the function called after a successful handshake.
   * @param callback Receives the new connection id.
   */
  void setOnClientConnected(ConnectionCallback callback);

  /**
   * @brief Sets the function called when a client disconnects.
   * @param callback Receives the closed connection id.
   */
  void setOnClientDisconnected(ConnectionCallback callback);

  /**
   * @brief Sets the function called for every game level message.
   * @param callback Receives the connection id, message type and payload.
   */
  void setOnMessage(MessageCallback callback);

private:
  void handleHandshake(const asio::ip::udp::endpoint &endpoint,
                       const std::vector<std::uint8_t> &data);
  void handleConnectedPacket(const asio::ip::udp::endpoint &endpoint,
                             const std::vector<std::uint8_t> &data);
  Connection *findConnectionByEndpoint(const asio::ip::udp::endpoint &endpoint);
  void handleDatagram(const asio::ip::udp::endpoint &endpoint,
                      const std::vector<std::uint8_t> &data);
  void sendWelcome(Connection &connection);
  void doSend(std::uint32_t connectionId,
              const std::vector<std::uint8_t> &payload,
              PacketPriority priority = PacketPriority::Unreliable);
  void flush(std::uint32_t connectionId);
  void startFlushTimer();
  void handleMessage(std::uint32_t connectionId, const ParsedMessage &message);
  void disconnectClient(std::uint32_t connectionId);

  asio::io_context _io;
  NetworkMetrics _metrics;
  UdpTransport _transport;
  asio::steady_timer _flushTimer;
  std::thread _networkThread;
  std::unordered_map<std::uint32_t, std::unique_ptr<Connection>> _connections;
  std::uint32_t _nextId = 1;
  std::atomic_bool _running{false};

  ConnectionCallback _onClientConnected;
  ConnectionCallback _onClientDisconnected;
  MessageCallback _onMessage;
};

/** @} */
