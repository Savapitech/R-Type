#pragma once

/**
 * @addtogroup api_network
 * @{
 */

#include <asio.hpp>
#include <asio/ip/udp.hpp>
#include <cstdint>

#include "../protocol/PacketHeader.hpp"
#include "../tracking/PacketTracker.hpp"

/**
 * @brief State the server keeps for one connected client.
 */
class Connection {
public:
  /**
   * @brief Creates a connection bound to a client address.
   * @param id Connection id given to the client.
   * @param endpoint Client address and port.
   */
  Connection(std::uint32_t id, const asio::ip::udp::endpoint &endpoint);

  /**
   * @brief Returns the connection id.
   * @return The id sent to the client in Welcome.
   */
  std::uint32_t getId() const;

  /**
   * @brief Returns the client address.
   * @return The endpoint the connection is bound to.
   */
  const asio::ip::udp::endpoint &getEndpoint() const;

  /**
   * @brief Builds the header of the next outgoing packet.
   * @return A header with a fresh sequence number and the current acks.
   */
  PacketHeader makeHeader();

  /**
   * @brief Records an incoming packet in the acknowledgement state.
   * @param header Header of the received packet.
   * @return Whether the packet is new, duplicated, out of order or too old.
   */
  PacketReceiveRes onPacketReceived(const PacketHeader &header);

  /**
   * @brief Returns the tracker that queues, resends and acknowledges packets.
   * @return The tracker of this connection.
   */
  PacketTracker &getTracker();

private:
  std::uint32_t _id;
  asio::ip::udp::endpoint _endpoint;
  PacketTracker _tracker;
};

/** @} */
