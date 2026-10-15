#pragma once

/**
 * @addtogroup api_network
 * @{
 */

#include "PacketReader.hpp"
#include "PacketWriter.hpp"
#include <cstddef>
#include <cstdint>

/// Numeric identifier of a message type.
using MessageId = std::uint16_t;

/**
 * @brief Header placed in front of every message inside a connected packet.
 *
 * | Offset | Size | Field  | Description                        |
 * | -----: | ---: | ------ | ---------------------------------- |
 * | 0      | 2    | `type` | Message type identifier.           |
 * | 2      | 2    | `size` | Size of the payload that follows.  |
 */
struct MessageHeader {
public:
  /**
   * @brief Writes the header in network byte order.
   * @param writer Destination buffer.
   * @return True on success, false if the buffer is full.
   */
  bool serialize(PacketWriter &writer) const;

  /**
   * @brief Reads the header from a buffer.
   * @param reader Source buffer.
   * @return True on success, false if fewer than 4 bytes remain.
   */
  bool deserialize(PacketReader &reader);

  /**
   * @brief Returns the message type.
   * @return The message type identifier.
   */
  MessageId getType() const;

  /**
   * @brief Sets the message type.
   * @param type The message type identifier.
   */
  void setType(MessageId type);

  std::uint16_t size = 0; ///< Payload size in bytes, header excluded.

  /// Encoded size of the header, in bytes.
  static constexpr std::size_t HEADER_SIZE = 4;

private:
  MessageId _type = 0;
};

/** @} */
