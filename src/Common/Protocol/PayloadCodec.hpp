#pragma once

#include <cstdint>
#include <type_traits>

#include "protocol/PacketReader.hpp"
#include "protocol/PacketWriter.hpp"

namespace rtype::protocol {
/**
 * @addtogroup api_protocol
 * @{
 */

/**
 * @brief Appends the raw bytes of a padding free payload struct.
 * @tparam T Payload type, made of single bytes and BigEndian fields.
 * @param writer Destination buffer.
 * @param payload Payload to write.
 * @return True on success, false if the buffer is full.
 */
template <typename T>
  requires std::is_trivially_copyable_v<T>
bool writePayload(PacketWriter &writer, const T &payload) {
  return writer.writeBytes(reinterpret_cast<const std::uint8_t *>(&payload),
                           sizeof(T));
}

/**
 * @brief Reads the raw bytes of a padding free payload struct.
 * @tparam T Payload type, made of single bytes and BigEndian fields.
 * @param reader Source buffer.
 * @param payload Receives the payload.
 * @return True on success, false if not enough bytes remain.
 */
template <typename T>
  requires std::is_trivially_copyable_v<T>
bool readPayload(PacketReader &reader, T &payload) {
  return reader.readBytes(reinterpret_cast<std::uint8_t *>(&payload),
                          sizeof(T));
}

/** @} */
} // namespace rtype::protocol
