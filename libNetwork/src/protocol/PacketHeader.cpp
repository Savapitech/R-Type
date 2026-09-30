// #include <network/protocol/PacketHeader.hpp>
#include "../../include/network/protocol/PacketHeader.hpp"

#include <iostream>

bool PacketHeader::serialize(PacketWriter &writer) const {
 return writer.writeUInt32(connectionId) && writer.writeUInt32(sequence) &&
   writer.writeUInt32(ack) && writer.writeUInt32(ackBits);
}

bool PacketHeader::deserialize(PacketReader &reader) {
  if (reader.getRemaining() < HEADER_SIZE)
    return false;

  std::uint32_t readConnectionId;
  std::uint32_t readSequence;
  std::uint32_t readAck;
  std::uint32_t readAckBits;

  if (!reader.readUInt32(readConnectionId) || !reader.readUInt32(readSequence) ||
    !reader.readUInt32(readAck) || !reader.readUInt32(readAckBits)) {
    std::cerr << "READ FAILED\n";
    return false;
  }

  connectionId = readConnectionId;
  sequence = readSequence;
  ack = readAck;
  ackBits = readAckBits;
  return true;
}
