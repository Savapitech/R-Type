#pragma once

#include <cstdint>
#include <type_traits>

#include "protocol/PacketReader.hpp"
#include "protocol/PacketWriter.hpp"

namespace rtype::protocol {
template <typename T>
  requires std::is_trivially_copyable_v<T>
bool writePayload(PacketWriter &writer, const T &payload) {
  return writer.writeBytes(reinterpret_cast<const std::uint8_t *>(&payload),
                           sizeof(T));
}

template <typename T>
  requires std::is_trivially_copyable_v<T>
bool readPayload(PacketReader &reader, T &payload) {
  return reader.readBytes(reinterpret_cast<std::uint8_t *>(&payload),
                          sizeof(T));
}
} // namespace rtype::protocol
