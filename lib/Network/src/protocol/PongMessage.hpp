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
 * @brief Payload of a Pong message, 4 bytes, sent by the server in answer to
 * a Ping.
 *
 * | Offset | Size | Field       | Description                          |
 * | -----: | ---: | ----------- | ------------------------------------ |
 * | 0      | 4    | `timestamp` | Value copied from the matching Ping. |
 */
struct PongMessage {
  std::uint32_t timestamp = 0; ///< Value copied from the Ping.

  /// Encoded size of the payload, in bytes.
  static constexpr std::size_t SIZE = 4;

  /**
   * @brief Writes the payload in network byte order.
   * @param writer Destination buffer.
   * @return True on success, false if the buffer is full.
   */
  bool serialize(PacketWriter &writer) const;

  /**
   * @brief Reads the payload from a buffer.
   * @param reader Source buffer.
   * @return True on success, false if fewer than 4 bytes remain.
   */
  bool deserialize(PacketReader &reader);
};

/** @} */
