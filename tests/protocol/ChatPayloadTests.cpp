#include "Test.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include "Protocol/ChatPayload.hpp"

using namespace rtype::protocol;

TEST(ChatMessage, RoundTripsAPlayerMessage) {
  ChatMessagePayload payload;
  payload.roomId = 3;
  payload.senderId = 42;
  payload.text = "hello bydos!";
  PacketWriter writer;

  CHECK(writeChatMessage(writer, payload));
  PacketReader reader(writer.getData());
  ChatMessagePayload decoded;
  CHECK(readChatMessage(reader, decoded));
  CHECK_EQ(decoded.roomId, 3u);
  CHECK_EQ(decoded.senderId, 42u);
  CHECK_EQ(decoded.text, std::string("hello bydos!"));
}

TEST(ChatMessage, RoundTripsAServerNotice) {
  ChatMessagePayload payload;
  payload.roomId = 1;
  payload.senderId = SERVER_SENDER_ID;
  payload.text = "P7 LOST CONNECTION";
  PacketWriter writer;

  CHECK(writeChatMessage(writer, payload));
  PacketReader reader(writer.getData());
  ChatMessagePayload decoded;
  CHECK(readChatMessage(reader, decoded));
  CHECK_EQ(decoded.senderId, SERVER_SENDER_ID);
  CHECK_EQ(decoded.text, std::string("P7 LOST CONNECTION"));
}

TEST(ChatMessage, RejectsRoomZero) {
  ChatMessagePayload payload;
  payload.roomId = 0;
  payload.senderId = 1;
  payload.text = "hi";
  PacketWriter writer;

  CHECK_FALSE(writeChatMessage(writer, payload));
}

TEST(ChatMessage, RejectsTextWithUnsupportedCharacters) {
  const std::vector<std::uint8_t> bytes = {0, 0, 0, 1, 0, 0, 0, 2,
                                           2, 'h', '\n'};
  PacketReader reader(bytes);
  ChatMessagePayload decoded;

  CHECK_FALSE(readChatMessage(reader, decoded));
}
