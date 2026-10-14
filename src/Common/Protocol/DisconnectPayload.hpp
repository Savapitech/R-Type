#pragma once

#include <cstdint>

namespace rtype::protocol {

enum class DisconnectReason : std::uint8_t {
  ClientQuit = 0x01,
  Timeout = 0x02,
  ProtocolError = 0x03,
  ServerShutdown = 0x04
};

struct DisconnectPayload {
  DisconnectReason reason = DisconnectReason::ClientQuit;
};

static_assert(sizeof(DisconnectPayload) == sizeof(DisconnectReason),
              "DisconnectPayload must not contain padding");

constexpr bool isValid(const DisconnectPayload &payload) {
  return static_cast<std::uint8_t>(payload.reason) >=
             static_cast<std::uint8_t>(DisconnectReason::ClientQuit) &&
         static_cast<std::uint8_t>(payload.reason) <=
             static_cast<std::uint8_t>(DisconnectReason::ServerShutdown);
}

} // namespace rtype::protocol
