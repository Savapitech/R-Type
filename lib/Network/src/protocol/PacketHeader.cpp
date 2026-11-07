#include "PacketHeader.hpp"

bool PacketHeader::serialize(PacketWriter &writer) const {
  return writer.writeUInt32(connectionId) && writer.writeUInt32(sequence) &&
         writer.writeUInt32(ack) && writer.writeUInt32(ackBits) &&
         writer.writeUInt8(static_cast<std::uint8_t>(
             static_cast<std::uint8_t>(priority) |
             (compressed ? COMPRESSED_FLAG : 0))) &&
         writer.writeUInt16(reliableId);
}

bool PacketHeader::deserialize(PacketReader &reader) {
  if (reader.getRemaining() < HEADER_SIZE)
    return false;

  std::uint32_t readConnectionId;
  std::uint32_t readSequence;
  std::uint32_t readAck;
  std::uint32_t readAckBits;
  std::uint8_t readPriority;
  std::uint16_t readReliableId;

  if (!reader.readUInt32(readConnectionId) ||
      !reader.readUInt32(readSequence) || !reader.readUInt32(readAck) ||
      !reader.readUInt32(readAckBits) || !reader.readUInt8(readPriority) ||
      !reader.readUInt16(readReliableId)) {
    return false;
  }

  const bool readCompressed = (readPriority & COMPRESSED_FLAG) != 0;
  readPriority &= static_cast<std::uint8_t>(~COMPRESSED_FLAG);
  if (readPriority > static_cast<std::uint8_t>(PacketPriority::Unreliable))
    return false;

  connectionId = readConnectionId;
  sequence = readSequence;
  ack = readAck;
  ackBits = readAckBits;
  priority = static_cast<PacketPriority>(readPriority);
  reliableId = readReliableId;
  compressed = readCompressed;
  return true;
}
