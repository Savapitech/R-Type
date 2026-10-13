#pragma once

#include "PacketReader.hpp"
#include "PacketWriter.hpp"
#include <cstddef>
#include <cstdint>

using MessageId = std::uint16_t;

struct MessageHeader {
public:
  bool serialize(PacketWriter &writer) const;
  bool deserialize(PacketReader &reader);
  MessageId getType() const;
  void setType(MessageId type);
  std::uint16_t size = 0;
  static constexpr std::size_t HEADER_SIZE = 4;

private:
  MessageId _type = 0;
};
