#include "MessageParser.hpp"
#include "Compression.hpp"
#include "MessageHeader.hpp"

bool MessageParser::parse(PacketReader &reader,
                          std::vector<ParsedMessage> &messages) {
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

bool MessageParser::parseBody(bool compressed, PacketReader &reader,
                              std::vector<ParsedMessage> &messages) {
  if (!compressed)
    return parse(reader, messages);

  std::vector<std::uint8_t> packed(reader.getRemaining());
  if (!reader.readBytes(packed.data(), packed.size()))
    return false;

  std::vector<std::uint8_t> body;
  if (!decompressBody(packed.data(), packed.size(), body))
    return false;

  PacketReader bodyReader(body);
  return parse(bodyReader, messages);
}
