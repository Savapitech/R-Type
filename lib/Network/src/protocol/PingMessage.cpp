#include "PingMessage.hpp"
#include "PacketReader.hpp"
#include "PacketWriter.hpp"

bool PingMessage::serialize(PacketWriter &writer) const {
  return writer.writeUInt32(timestamp);
}

bool PingMessage::deserialize(PacketReader &reader) {
  return reader.readUInt32(timestamp);
}
