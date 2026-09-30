#include <network/protocol/HandshakeHeader.hpp>
#include <cstdint>
#include <iostream>

bool HandshakeHeader::serialize(PacketWriter &writer) const {
 return writer.writeUInt32(magic) && writer.writeUInt8(version) &&
   writer.writeUInt8(static_cast<uint8_t>(type));
}

bool HandshakeHeader::deserialize(PacketReader &reader) {
  if (reader.getRemaining() < HEADER_SIZE)
    return false;

  std::uint32_t readMagic;
  std::uint8_t readVersion;
  std::uint8_t readType;

  if (!reader.readUInt32(readMagic) ||
    !reader.readUInt8(readVersion) ||
    !reader.readUInt8(readType)) {
    std::cerr << "READ FAILED\n";
    return false;
  }

  if (readMagic != MAGIC)
    return false;

  if (readType < static_cast<std::uint8_t>(HandshakeType::Hello) ||
      readType > static_cast<std::uint8_t>(HandshakeType::Rejected))
    return false;

  magic = readMagic;
  version = readVersion;
  type = static_cast<HandshakeType>(readType);
  return true;
}
