#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "Protocol/InputPayload.hpp"
#include "protocol/MessageHeader.hpp"
#include "server/NetworkServer.hpp"

namespace rtype::server {
class Server {
public:
  explicit Server(std::uint16_t port);
  ~Server();

  void start();
  void stop();
  void send(std::uint32_t connectionId,
            const std::vector<std::uint8_t> &payload);

private:
  void handleMessage(std::uint32_t connectionId, MessageType type,
                     const std::vector<std::uint8_t> &payload);

  NetworkServer _network;

  std::function<void(std::uint32_t connectionId)> _onClientConnected;
  std::function<void(std::uint32_t connectionId)> _onClientDisconnected;
  std::function<void(std::uint32_t connectionId,
                     const protocol::InputPayload &input)>
      _onInput;
};
} // namespace rtype::server
