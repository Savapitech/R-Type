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
 * @brief Payload of a Ping message, 4 bytes, sent by a client to measure
 * latency.
 *
 * | Offset | Size | Field       | Description                          |
 * | -----: | ---: | ----------- | ------------------------------------ |
 * | 0      | 4    | `timestamp` | Opaque value echoed back in a Pong.  |
 */
struct PingMessage {
  std::uint32_t timestamp = 0; ///< Sender defined time value.

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
