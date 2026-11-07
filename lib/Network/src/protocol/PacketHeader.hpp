#pragma once

/**
 * @addtogroup api_network
 * @{
 */

#include "../tracking/PacketPriority.hpp"
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
 * | 16     | 1    | `priority`     | Bits 0-6: PacketPriority (0 to 2).       |
 * |        |      |                | Bit 7: body is compressed (see below).   |
 * | 17     | 2    | `reliableId`   | Id kept by every resend, 0 if priority 2.
 * |
 */
struct PacketHeader {
  std::uint32_t connectionId = 0; ///< Connection the packet belongs to.
  std::uint32_t sequence = 0;     ///< Sequence number of this packet.
  std::uint32_t ack = 0;          ///< Latest sequence received from the peer.
  std::uint32_t ackBits = 0;      ///< Bitfield of the 32 previous sequences.
  /// Delivery guarantee requested for the packet.
  PacketPriority priority = PacketPriority::Unreliable;
  std::uint16_t reliableId = 0; ///< Same value on every resend of a packet.
  /// True if the body that follows is a zstd frame, see Compression.hpp.
  bool compressed = false;

  /// Bit of the priority byte that tells that the body is compressed.
  static constexpr std::uint8_t COMPRESSED_FLAG = 0x80;

  /// Encoded size of the header, in bytes.
  static constexpr std::size_t HEADER_SIZE = 19;

  /**
   * @brief Writes the header in network byte order.
   * @param writer Destination buffer.
   * @return True on success, false if the buffer is full.
   */
  bool serialize(PacketWriter &writer) const;

  /**
   * @brief Reads the header from a buffer.
   * @param reader Source buffer.
   * @return True on success, false if fewer than 19 bytes, or if the priority
   * is invalid remain.
   * The compressed flag is bit 7 of the priority byte.
   */
  bool deserialize(PacketReader &reader);
};

/** @} */
