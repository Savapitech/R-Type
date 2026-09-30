#pragma once

#include <cstddef>
#include <cstdint>

struct PacketHeader {
  std::uint32_t connectionId = 0;
  std::uint32_t sequence = 0;
  std::uint32_t ack = 0;
  std::uint32_t ackBits = 0;

  static constexpr std::size_t HEADER_SIZE = 16;
};
