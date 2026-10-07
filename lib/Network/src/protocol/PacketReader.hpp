#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

class PacketReader {
  public:
    PacketReader(const std::uint8_t *data, std::size_t size);
    PacketReader(const std::vector<uint8_t> &data);
    bool readUInt8(std::uint8_t &value);
    bool readUInt16(std::uint16_t &value);
    bool readUInt32(std::uint32_t &value);
    bool readBytes(std::uint8_t *destination, std::size_t size);

    std::size_t getOffset() const;
    std::size_t getRemaining() const;
    bool isEnd() const;
    void reset();

  private:
    bool canRead(std::size_t size) const;

    const std::uint8_t *_data;
    std::size_t _size;
    std::size_t _offset;
};
