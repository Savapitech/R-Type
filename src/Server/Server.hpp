#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include <mutex>
#include <queue>
#include <unordered_map>

#include "Protocol/EntityPayload.hpp"
#include "Protocol/InputPayload.hpp"
#include "Protocol/MessageType.hpp"
#include "Protocol/SnapshotPayload.hpp"
#include "protocol/MessageHeader.hpp"
#include "server/NetworkServer.hpp"

namespace rtype::server {
class Server {
public:
  explicit Server(std::uint16_t port);
  ~Server();
  struct InputEvent {
    std::uint32_t connectionId;
    protocol::InputPayload input;
  };

  struct ConnectionEvent {
    enum class Type { Connected, Disconnected } type;
    std::uint32_t connectionId;
  };

  struct QueryEntityEvent {
    std::uint32_t connectionId;
    std::uint32_t networkId;
  };

  void start();
  void stop();
  void send(std::uint32_t connectionId,
            const std::vector<std::uint8_t> &payload);
  bool pollInput(InputEvent &event);
  bool pollConnectionEvent(ConnectionEvent &event);
  bool pollQueryEntity(QueryEntityEvent &event);

  bool sendCreateEntity(std::uint32_t connectionId, const protocol::CreateEntityPayload &payload);
  bool sendDestroyEntity(std::uint32_t connectionId, const protocol::DestroyEntityPayload &payload);
  bool sendSnapshot(std::uint32_t connectionId, const protocol::SnapshotPayload &payload);

private:
  void handleMessage(std::uint32_t connectionId, MessageId type,
                     const std::vector<std::uint8_t> &payload);
  bool sendMessage(std::uint32_t connectionId, protocol::MessageType type, const std::vector<std::uint8_t> &payload);

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
} // namespace rtype::server
