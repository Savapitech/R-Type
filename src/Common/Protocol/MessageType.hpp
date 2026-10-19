#pragma once

#include <cstdint>

#include "protocol/SystemMessages.hpp"
#include "tracking/PacketPriority.hpp"

namespace rtype::protocol {
/**
 * @addtogroup api_protocol
 * @{
 */

/**
 * @brief Type identifier of every message exchanged on a connection.
 */
enum class MessageType : std::uint16_t {
  Invalid = 0, ///< Never sent, marks an unset type.

  Input = 0x02,       ///< Client to server: actions held by the player.
  Ping = 0x03,        ///< Client to server: latency probe.
  Disconnect = 0x04,  ///< Client to server: closes the connection.
  QueryEntity = 0x13, ///< Client to server: asks for an unknown entity.

  Snapshot = 0x83,      ///< Server to client: positions of the entities.
  CreateEntity = 0x84,  ///< Server to client: spawns an entity.
  DestroyEntity = 0x85, ///< Server to client: removes an entity.
  Pong = 0x86           ///< Server to client: answer to a Ping.
};

/**
 * @brief Converts a message type to its wire identifier.
 * @param type Message type.
 * @return The 16-bit identifier written in the message header.
 */
constexpr std::uint16_t toMessageId(MessageType type) {
  return static_cast<std::uint16_t>(type);
}

/**
 * @brief Delivery guarantee used to send a message type.
 * @param type Message type.
 * @return The priority to enqueue the message with.
 */
constexpr PacketPriority messagePriority(MessageType type) {
  switch (type) {
  case MessageType::CreateEntity:
  case MessageType::DestroyEntity:
    return PacketPriority::Critical;
  case MessageType::QueryEntity:
    return PacketPriority::Reliable;
  default:
    return PacketPriority::Unreliable;
  }
}

/** @} */
} // namespace rtype::protocol
