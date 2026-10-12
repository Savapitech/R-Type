#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include <mutex>
#include <queue>
#include <unordered_map>

#include "Protocol/InputPayload.hpp"
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

  void start();
  void stop();
  void send(std::uint32_t connectionId,
            const std::vector<std::uint8_t> &payload);
  bool pollInput(InputEvent &event);

private:
  void handleMessage(std::uint32_t connectionId, MessageType type,
                     const std::vector<std::uint8_t> &payload);

  NetworkServer _network;
  std::mutex _inputMutex;
  std::queue<InputEvent> _inputQueue;
  std::unordered_map<std::uint32_t, std::uint32_t> _lastInputSequence;

  std::function<void(std::uint32_t connectionId)> _onClientConnected;
  std::function<void(std::uint32_t connectionId)> _onClientDisconnected;
};
} // namespace rtype::server
