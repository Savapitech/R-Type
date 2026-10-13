#pragma once

#include <asio/io_context.hpp>
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

class NetworkServer {
public:
  using ConnectionCallback = std::function<void(std::uint32_t connectionId)>;
  using MessageCallback =
      std::function<void(std::uint32_t connectionId, MessageId type,
                         const std::vector<std::uint8_t> &payload)>;

  NetworkServer(std::uint16_t port);
  ~NetworkServer();
  void start();
  void stop();
  void send(std::uint32_t connectionId,
            const std::vector<std::uint8_t> &payload);

  // NOTE: those callbacks are invoked from the internal network thread (asio),
  // never from the thread that calls start() or stop(). The caller is
  // responsible for any synchronization needed to use the data on another
  // thread (eg. the ECS/game loop thread).
  void setOnClientConnected(ConnectionCallback callback);
  void setOnClientDisconnected(ConnectionCallback callback);
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
              const std::vector<std::uint8_t> &payload);
  void handleMessage(std::uint32_t connectionId, const ParsedMessage &message);
  void disconnectClient(std::uint32_t connectionId);

  asio::io_context _io;
  NetworkMetrics _metrics;
  UdpTransport _transport;
  std::thread _networkThread;
  std::unordered_map<std::uint32_t, std::unique_ptr<Connection>> _connections;
  std::uint32_t _nextId = 1;
  std::atomic_bool _running{false};

  ConnectionCallback _onClientConnected;
  ConnectionCallback _onClientDisconnected;
  MessageCallback _onMessage;
};
