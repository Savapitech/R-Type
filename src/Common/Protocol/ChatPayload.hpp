#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "protocol/PacketReader.hpp"
#include "protocol/PacketWriter.hpp"

namespace rtype::protocol {
/**
 * @addtogroup api_protocol
 * @{
 */

/**
 * @brief Maximum length of a chat message, in characters.
 */
inline constexpr std::size_t MAX_CHAT_MESSAGE_LENGTH = 64;

/// Sender id of a ChatMessage written by the server itself
inline constexpr std::uint32_t SERVER_SENDER_ID = 0;

/**
 * @brief ChatSend request payload: a message typed by a player.
 */
struct ChatSendPayload {
  std::string text; ///< Message to broadcast to the room.
};

/**
 * @brief ChatMessage notification payload: a message sent in the room.
 */
struct ChatMessagePayload {
  std::uint32_t roomId = 0;   ///< Room the message was sent in.
  std::uint32_t senderId = 0; ///< Connection ID of the sender, or
                              ///< SERVER_SENDER_ID for a server notice.
  std::string text;           ///< Message content.
};

/**
 * @brief Tells whether a character can be sent in a chat message.
 *
 * Only the characters drawn by the game font are accepted.
 *
 * @param c Character to check.
 * @return True if the character is allowed.
 */
constexpr bool isChatCharacter(char c) {
  if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
      (c >= '0' && c <= '9'))
    return true;
  switch (c) {
  case ' ':
  case '!':
  case ',':
  case '.':
  case '/':
  case ';':
  case '?':
  case '_':
    return true;
  default:
    return false;
  }
}

/**
 * @brief Tells whether a text is a valid chat message.
 *
 * A valid message is not empty, not longer than MAX_CHAT_MESSAGE_LENGTH,
 * made only of chat characters, and not only spaces.
 *
 * @param text Text to check.
 * @return True if the text can be sent.
 */
constexpr bool isValidChatText(std::string_view text) {
  if (text.empty() || text.size() > MAX_CHAT_MESSAGE_LENGTH)
    return false;
  bool hasVisible = false;
  for (const char c : text) {
    if (!isChatCharacter(c))
      return false;
    if (c != ' ')
      hasVisible = true;
  }
  return hasVisible;
}

/**
 * @brief Writes a length-prefixed chat text.
 * @param writer Destination writer.
 * @param text Text to write.
 * @return True on success.
 */
inline bool writeChatText(PacketWriter &writer, const std::string &text) {
  if (!isValidChatText(text))
    return false;
  if (!writer.writeUInt8(static_cast<std::uint8_t>(text.size())))
    return false;
  return writer.writeBytes(reinterpret_cast<const std::uint8_t *>(text.data()),
                           text.size());
}

/**
 * @brief Reads and validates a length-prefixed chat text.
 * @param reader Source reader.
 * @param text Receives the text.
 * @return True if the text is valid.
 */
inline bool readChatText(PacketReader &reader, std::string &text) {
  std::uint8_t length = 0;
  if (!reader.readUInt8(length) || length > MAX_CHAT_MESSAGE_LENGTH)
    return false;
  std::string decoded(length, '\0');
  if (!reader.readBytes(reinterpret_cast<std::uint8_t *>(decoded.data()),
                        length))
    return false;
  if (!isValidChatText(decoded))
    return false;
  text = std::move(decoded);
  return true;
}

/**
 * @brief Encodes a ChatSend request.
 * @param writer Destination writer.
 * @param payload Request to encode.
 * @return True on success.
 */
inline bool writeChatSend(PacketWriter &writer,
                          const ChatSendPayload &payload) {
  return writeChatText(writer, payload.text);
}

/**
 * @brief Decodes a ChatSend request.
 * @param reader Source reader.
 * @param payload Receives the request.
 * @return True if the payload is valid.
 */
inline bool readChatSend(PacketReader &reader, ChatSendPayload &payload) {
  std::string text;
  if (!readChatText(reader, text) || !reader.isEnd())
    return false;
  payload.text = std::move(text);
  return true;
}

/**
 * @brief Encodes a ChatMessage notification.
 * @param writer Destination writer.
 * @param payload Notification to encode.
 * @return True on success.
 */
inline bool writeChatMessage(PacketWriter &writer,
                             const ChatMessagePayload &payload) {
  if (payload.roomId == 0)
    return false;
  return writer.writeUInt32(payload.roomId) &&
         writer.writeUInt32(payload.senderId) &&
         writeChatText(writer, payload.text);
}

/**
 * @brief Decodes a ChatMessage notification.
 * @param reader Source reader.
 * @param payload Receives the notification.
 * @return True if the payload is valid.
 */
inline bool readChatMessage(PacketReader &reader, ChatMessagePayload &payload) {
  ChatMessagePayload decoded;
  if (!reader.readUInt32(decoded.roomId) ||
      !reader.readUInt32(decoded.senderId) || decoded.roomId == 0 ||
      !readChatText(reader, decoded.text) || !reader.isEnd())
    return false;
  payload = std::move(decoded);
  return true;
}

/** @} */
} // namespace rtype::protocol
