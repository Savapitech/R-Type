#pragma once

#include <cstdint>

namespace rtype::events {
/**
 * @addtogroup api_engine
 * @{
 */

/**
 * @brief Published when the server accepted the handshake.
 */
struct ConnectedToServerEvent {
  std::uint32_t clientId = 0;        ///< Connection id given by the server.
  std::uint32_t playerNetworkId = 0; ///< Network id of the local ship.
  std::uint32_t serverTick = 0;      ///< Server tick at connection time.
};

/**
 * @brief Published when the server refused the handshake.
 */
struct ConnectionRejectedEvent {
  std::uint8_t reason = 0; ///< Refusal reason.
};

/**
 * @brief Asks the network client to send raw bytes to the server.
 */
struct SendPacketEvent {
  std::vector<std::uint8_t> data; ///< Datagram to send.
};

/**
 * @brief Decoded CreateEntity message in world units.
 */
struct CreateEntityEvent {
  std::uint32_t serverTick; ///< Server tick at sending time.
  std::uint32_t networkId;  ///< Identifier of the entity.
  std::uint16_t entityType; ///< Archetype id of the entity blueprint.
  float positionX;          ///< Horizontal position in pixels.
  float positionY;          ///< Vertical position in pixels.
  float rotation;           ///< Rotation in radians.
};

/** @} */
} // namespace rtype::events
