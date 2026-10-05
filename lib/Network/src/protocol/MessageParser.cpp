#include "MessageParser.hpp"
#include "MessageHeader.hpp"

bool MessageParser::parse(PacketReader &reader, std::vector<ParsedMessage> &messages) {
  std::vector<ParsedMessage> parsed;

  while (!reader.isEnd()) {
    if (reader.getRemaining() < MessageHeader::HEADER_SIZE)
      return false;

    MessageHeader header;
    if (!header.deserialize(reader))
      return false;

    if (header.size > reader.getRemaining())
      return false;

    ParsedMessage mess;
    mess.type = header.getType();
    mess.payload.resize(header.size);

    if (!reader.readBytes(mess.payload.data(), header.size))
      return false;

    parsed.push_back(std::move(mess));
  }
  messages = std::move(parsed);
  return true;
}
