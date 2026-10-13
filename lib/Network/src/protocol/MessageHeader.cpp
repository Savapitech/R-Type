#include "MessageHeader.hpp"
#include <cstdint>

bool MessageHeader::serialize(PacketWriter &writer) const {
  if (!writer.writeUInt16(static_cast<std::uint16_t>(getType())))
    return false;
  if (!writer.writeUInt16(size))
    return false;
  return true;
}

bool MessageHeader::deserialize(PacketReader &reader) {
  std::uint16_t rawType = 0;
  if (!reader.readUInt16(rawType))
    return false;
  if (!reader.readUInt16(size))
    return false;

  setType(static_cast<MessageId>(rawType));
  return true;
}

void MessageHeader::setType(MessageId type) { _type = type; }

MessageId MessageHeader::getType() const { return _type; }
