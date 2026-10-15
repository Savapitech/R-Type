#pragma once

/**
 * @addtogroup api_network
 * @{
 */

#include "PacketReader.hpp"
#include "PacketWriter.hpp"
#include <cstddef>
#include <cstdint>

/// Magic number that starts every handshake datagram, "RTYP" in ASCII.
constexpr std::uint32_t MAGIC = 0x52545950;
/// Protocol version announced and expected during the handshake.
constexpr std::uint8_t VERSION = 1;

/**
 * @brief Kind of handshake datagram.
 */
enum class HandshakeType : std::uint8_t {
  Hello = 1,    ///< Client asks the server to open a connection.
  Welcome = 2,  ///< Server accepts the client and gives its connection id.
  Rejected = 3, ///< Server refuses the connection.
};

/**
 * @brief Header of the datagrams exchanged before a connection exists.
 *
 * | Offset | Size | Field     | Description                          |
 * | -----: | ---: | --------- | ------------------------------------ |
 * | 0      | 4    | `magic`   | Always `0x52545950`.                 |
 * | 4      | 1    | `version` | Protocol version, currently `1`.     |
 * | 5      | 1    | `type`    | A HandshakeType value.               |
 *
 * A Welcome datagram is followed by the 4 byte connection id.
 */
struct HandshakeHeader {
  std::uint32_t magic = MAGIC;               ///< Protocol magic number.
  std::uint8_t version = VERSION;            ///< Protocol version.
  HandshakeType type = HandshakeType::Hello; ///< Handshake step.

  /// Encoded size of the header, in bytes.
  static constexpr std::size_t HEADER_SIZE = 6;

  /**
   * @brief Writes the header in network byte order.
   * @param writer Destination buffer.
   * @return True on success, false if the buffer is full.
   */
  bool serialize(PacketWriter &writer) const;

  /**
   * @brief Reads and validates the magic number and handshake type.
   * @param reader Source buffer.
   * @return True if a valid header was read, false otherwise.
   */
  bool deserialize(PacketReader &reader);
};

/** @} */
