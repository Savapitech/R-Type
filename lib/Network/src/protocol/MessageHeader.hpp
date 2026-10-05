#pragma once 

#include "PacketReader.hpp"
#include "PacketWriter.hpp"
#include <cstddef>
#include <cstdint>

enum class MessageType : std::uint16_t {
  Invalid = 0,
  Input = 0x01,
  Ping = 0x02,
  Disconnect = 0x03
};

struct MessageHeader {
  public:
    bool serialize(PacketWriter &writer) const;
    bool deserialize(PacketReader &reader);
    MessageType getType() const;
    void setType(MessageType type);
    std::uint16_t size = 0;
    static constexpr std::size_t HEADER_SIZE = 4;
  private:
    MessageType _type = MessageType::Invalid;
};
