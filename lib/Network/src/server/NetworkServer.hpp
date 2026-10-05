#pragma once

#include <asio/io_context.hpp>
#include <cstdint>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>
#include <atomic>

#include "../connection/Connection.hpp"
#include "../metrics/Metrics.hpp"
#include "../transport/UdpTransport.hpp"
#include "../protocol/MessageParser.hpp"

class NetworkServer {
  public:
    NetworkServer(std::uint16_t port);
    ~NetworkServer();
    void start();
    void stop();
    void send(std::uint32_t connectionId, const std::vector<std::uint8_t> &payload);
  
  private:  
    void handleHandshake(const asio::ip::udp::endpoint &endpoint, const std::vector<std::uint8_t> &data);
    void handleConnectedPacket(const asio::ip::udp::endpoint &endpoint, const std::vector<std::uint8_t> &data);
    Connection *findConnectionByEndpoint(const asio::ip::udp::endpoint &endpoint);
    void handleDatagram(const asio::ip::udp::endpoint &endpoint, const std::vector<std::uint8_t> &data);
    void sendWelcome(Connection &connection);
    void doSend(std::uint32_t connectionId, const std::vector<std::uint8_t> &payload);
    void handleMessage(std::uint32_t connectionId, const ParsedMessage &message);
    
    asio::io_context _io;
    NetworkMetrics _metrics;
    UdpTransport _transport;
    std::thread _networkThread;
    std::unordered_map<std::uint32_t, std::unique_ptr<Connection>> _connections;
    std::uint32_t _nextId = 1;
    std::atomic_bool _running{false};
};
