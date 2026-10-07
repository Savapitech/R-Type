#pragma once 

#include "MessageHeader.hpp"
#include "PacketReader.hpp"

#include <cstdint>
#include <vector>

struct ParsedMessage {
  MessageType type = MessageType::Invalid;
  std::vector<std::uint8_t> payload;
};

class MessageParser {
  public:
    static bool parse(PacketReader &reader, std::vector<ParsedMessage> &messages);
};
