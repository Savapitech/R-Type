#pragma once

#include <cstdint>

#include "protocol/SystemMessages.hpp"

namespace rtype::protocol {

enum class MessageType : std::uint16_t {
  Invalid = 0,

  // Client -> server
  Input = 0x02,
  Ping = 0x03,
  Disconnect = 0x04,
  QueryEntity = 0x13,

  // Server -> client
  Snapshot = 0x83,
  CreateEntity = 0x84,
  DestroyEntity = 0x85,
  Pong = 0x86
};

constexpr std::uint16_t toMessageId(MessageType type) {
  return static_cast<std::uint16_t>(type);
}

} // namespace rtype::protocol
