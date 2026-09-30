#include <network/protocol/PacketReader.hpp>
#include <algorithm>
#include <cstddef>
#include <cstdint>

PacketReader::PacketReader(const std::uint8_t *data, std::size_t size) :
  _data(data), _size(size), _offset(0)
{}

PacketReader::PacketReader(const std::vector<uint8_t> &data) :
  PacketReader(data.data(), data.size())
{}

bool PacketReader::canRead(std::size_t size) const {
  if(_offset > size)
    return false;
  return size <= (_size - _offset);
}

bool PacketReader::readUInt8(std::uint8_t &value) {
  if (!canRead(1))
    return false;

  value = _data[_offset];
  _offset++;
  return true;
}

bool PacketReader::readUInt16(std::uint16_t &value) {
  if (!canRead(2))
    return false;

  value = (static_cast<uint16_t>(_data[_offset]) << 8 | static_cast<uint16_t>(_data[_offset + 1]));
  _offset += 2;
  return true;
}

bool PacketReader::readUInt32(std::uint32_t &value) {
  if (!canRead(4))
    return false;

  value = (static_cast<uint16_t>(_data[_offset]) << 24 |
      static_cast<uint16_t>(_data[_offset + 1]) << 16 | 
      static_cast<uint16_t>(_data[_offset + 2]) << 8 | 
      static_cast<uint16_t>(_data[_offset + 3])); 
  _offset += 4;
  return true;
}

bool PacketReader::readBytes(std::uint8_t *destination, std::size_t size) {
  if (destination == nullptr && size != 0) {
    return false;
  }

  if (!canRead(size))
    return false;
  
  std::copy(_data + _offset, _data + _offset + size, destination);
  _offset += size;
  return true;
}

std::size_t PacketReader::getOffset() const
{
    return _offset;
}

std::size_t PacketReader::getRemaining() const
{
    return (_size - _offset);
}

bool PacketReader::isEnd() const
{
    return _offset >= _size;
}

void PacketReader::reset()
{
    _offset = 0;
}
