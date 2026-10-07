#pragma once 

#include <cstddef>
#include <cstdint>
#include <vector>
#include "../config/NetworkConfig.hpp"

class PacketWriter {
  public:

    PacketWriter();
    bool writeUInt8(std::uint8_t value);
    bool writeUInt16(std::uint16_t value);
    bool writeUInt32(std::uint32_t value);
    bool writeBytes(const std::uint8_t *data, std::size_t size);
    bool empty() const;
    void clear();

    const std::vector<std::uint8_t> &getData() const;
    std::size_t getSize() const;

  private:
    bool canWrite(std::size_t size) const;
    std::vector<std::uint8_t> _buffer;
};
