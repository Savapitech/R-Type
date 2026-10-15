#pragma once

/**
 * @addtogroup api_network
 * @{
 */

#include "MessageHeader.hpp"
#include "PacketReader.hpp"

#include <cstdint>
#include <vector>

/**
 * @brief Message extracted from a connected packet.
 */
struct ParsedMessage {
  MessageId type = 0;                ///< Message type identifier.
  std::vector<std::uint8_t> payload; ///< Raw payload bytes.
};

/**
 * @brief Splits the body of a connected packet into messages.
 */
class MessageParser {
public:
  /**
   * @brief Parses every message until the end of the reader.
   * @param reader Buffer positioned right after the packet header.
   * @param messages Receives the messages, left untouched on failure.
   * @return True if the whole body was valid, false otherwise.
   */
  static bool parse(PacketReader &reader, std::vector<ParsedMessage> &messages);
};

/** @} */
