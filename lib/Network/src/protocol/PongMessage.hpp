#pragma once 

#include "PacketReader.hpp"
#include "PacketWriter.hpp"

#include <cstddef>
#include <cstdint>

struct PongMessage {
  std::uint32_t timestamp = 0;

  static constexpr std::size_t SIZE = 4;

  bool serialize(PacketWriter &writer) const;
  bool deserialize(PacketReader &reader);
};
