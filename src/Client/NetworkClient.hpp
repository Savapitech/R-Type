#pragma once

#include <array>
#include <asio.hpp>
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

enum class NetworkEventType { Connected, Rejected, Disconnected, Message };

struct NetworkEvent {
  NetworkEventType type = NetworkEventType::Message;
  std::uint32_t connectionId = 0;
  rtype::protocol::MessageType messageType = protocol::MessageType::Invalid;
  std::vector<std::uint8_t> payload;
};

using ReplicationPayload =
    std::variant<protocol::CreateEntityPayload, protocol::DestroyEntityPayload,
                 protocol::SnapshotPayload>;

struct ReplicationEvent {
  std::uint32_t connectionId = 0;
  ReplicationPayload payload;
};

class NetworkClient {
private:
  asio::io_context _ioContext;
  asio::ip::udp::socket _socket;
  asio::ip::udp::endpoint _serverEndpoint;
  asio::ip::udp::endpoint _receiveEndpoint;
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
  void pushEvent(NetworkEvent event);

public:
  NetworkClient();
  ~NetworkClient();
  NetworkClient(const NetworkClient &) = delete;
  NetworkClient &operator=(const NetworkClient &) = delete;
  bool connect(const std::string &host, const std::string &port);
  void sendHello();
  void sendInput(std::uint32_t clientTick,
                 rtype::protocol::InputAction actions);
  void sendQueryEntity(std::uint32_t networkId);
  void send(const std::vector<std::uint8_t> &data);
  bool pollEvent(NetworkEvent &event);
  bool pollReplicationEvent(ReplicationEvent &event);
  bool isConnected() const;
  bool isRunning() const;
  std::uint32_t getConnectionId() const;
  void stop();
};
} // namespace rtype::client
