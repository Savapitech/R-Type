#pragma once

/**
 * @addtogroup api_network
 * @{
 */

#include "PacketReader.hpp"
#include "PacketWriter.hpp"
#include <cstddef>
#include <cstdint>

/**
 * @brief Header that starts every packet sent on an open connection.
 *
 * | Offset | Size | Field          | Description                              |
 * | -----: | ---: | -------------- | ---------------------------------------- |
 * | 0      | 4    | `connectionId` | Id given by the server in Welcome.       |
 * | 4      | 4    | `sequence`     | Sequence number of this packet.          |
 * | 8      | 4    | `ack`          | Latest sequence received from the peer.  |
 * | 12     | 4    | `ackBits`      | Bit i set if `ack - 1 - i` was received. |
 */
struct PacketHeader {
  std::uint32_t connectionId = 0; ///< Connection the packet belongs to.
  std::uint32_t sequence = 0;     ///< Sequence number of this packet.
  std::uint32_t ack = 0;          ///< Latest sequence received from the peer.
  std::uint32_t ackBits = 0;      ///< Bitfield of the 32 previous sequences.

  /// Encoded size of the header, in bytes.
  static constexpr std::size_t HEADER_SIZE = 16;

  /**
   * @brief Writes the header in network byte order.
   * @param writer Destination buffer.
   * @return True on success, false if the buffer is full.
   */
  bool serialize(PacketWriter &writer) const;

  /**
   * @brief Reads the header from a buffer.
   * @param reader Source buffer.
   * @return True on success, false if fewer than 16 bytes remain.
   */
  bool deserialize(PacketReader &reader);
};

/** @} */
