#pragma once

#include <cstdint>

namespace rtype::protocol {

enum class MessageType : std::uint16_t {
  Invalid = 0,
  Input = 0x01,
  Ping = 0x02,
  Disconnect = 0x03,
  Pong = 0x04,
  CreateEntity = 0x10,
  DestroyEntity = 0x11,
  Snapshot = 0x12,
  QueryEntity = 0x13
};

constexpr std::uint16_t toMessageId(MessageType type) {
  return static_cast<std::uint16_t>(type);
}

} // namespace rtype::protocol
