#include "../../include/network/protocol/PacketWriter.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

PacketWriter::PacketWriter() {
  _buffer.reserve(MAX_PACKET_SIZE);
}

bool PacketWriter::canWrite(std::size_t size) const {
  return _buffer.size() + size <= MAX_PACKET_SIZE;
}

bool PacketWriter::writeUInt8(std::uint8_t value) {
  if(!canWrite(sizeof(value)))
    return false;

  _buffer.push_back(value);
  return true;
}

bool PacketWriter::writeUInt16(std::uint16_t value) {
  if(!canWrite(sizeof(value)))
    return false;

  _buffer.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
  _buffer.push_back(static_cast<std::uint8_t>(value & 0xFF));
  return true;
}

bool PacketWriter::writeUInt32(std::uint32_t value) {
  if(!canWrite(sizeof(value)))
    return false;

  _buffer.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
  _buffer.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
  _buffer.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
  _buffer.push_back(static_cast<std::uint8_t>(value & 0xFF));
  return true;
}

bool PacketWriter::writeBytes(const std::uint8_t *data, std::size_t size) {
  if (data == nullptr && size != 0) {
    return false;
  }

  if (!canWrite(size))
    return false;

  _buffer.insert(_buffer.end(), data, data + size);
  return true;
}

const std::vector<std::uint8_t> &PacketWriter::getData() const {
  return _buffer;
}

bool PacketWriter::empty() const {
  return _buffer.empty();
}

std::size_t PacketWriter::getSize() const {
  return _buffer.size();
}

void PacketWriter::clear() {
  _buffer.clear();
}


