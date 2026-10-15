#pragma once

#include <cstdint>

namespace rtype::protocol {
/**
 * @addtogroup api_protocol
 * @{
 */

/**
 * @brief Reason carried by a Disconnect message.
 */
enum class DisconnectReason : std::uint8_t {
  ClientQuit = 0x01,    ///< The player left the game.
  Timeout = 0x02,       ///< The peer stopped answering.
  ProtocolError = 0x03, ///< The peer sent invalid data.
  ServerShutdown = 0x04 ///< The server is stopping.
};

/**
 * @brief Payload of a Disconnect message, 1 byte.
 *
 * | Offset | Size | Field    | Description                      |
 * | -----: | ---: | -------- | -------------------------------- |
 * | 0      | 1    | `reason` | A DisconnectReason value.        |
 */
struct DisconnectPayload {
  /// Why the connection closes.
  DisconnectReason reason = DisconnectReason::ClientQuit;
};

static_assert(sizeof(DisconnectPayload) == sizeof(DisconnectReason),
              "DisconnectPayload must not contain padding");

/**
 * @brief Checks that the reason is a known value.
 * @param payload Decoded payload.
 * @return True if the payload is valid.
 */
constexpr bool isValid(const DisconnectPayload &payload) {
  return static_cast<std::uint8_t>(payload.reason) >=
             static_cast<std::uint8_t>(DisconnectReason::ClientQuit) &&
         static_cast<std::uint8_t>(payload.reason) <=
             static_cast<std::uint8_t>(DisconnectReason::ServerShutdown);
}

/** @} */
} // namespace rtype::protocol
