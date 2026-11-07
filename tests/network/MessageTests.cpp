#include "NetworkTestUtils.hpp"
#include "Test.hpp"

#include <cstdint>
#include <vector>

#include "config/NetworkConfig.hpp"
#include "protocol/MessageParser.hpp"
#include "protocol/PingMessage.hpp"
#include "protocol/PongMessage.hpp"
#include "protocol/SystemMessages.hpp"

using rtype::test::frame;
using rtype::test::noise;

// MessageParser

TEST(MessageParser, EmptyBodyGivesNoMessage) {
  const std::vector<std::uint8_t> body;
  PacketReader reader(body);
  std::vector<ParsedMessage> messages;

  CHECK(MessageParser::parse(reader, messages));
  CHECK(messages.empty());
}

TEST(MessageParser, ParsesOneMessage) {
  const auto body = frame(0x0042, {1, 2, 3});
  PacketReader reader(body);
  std::vector<ParsedMessage> messages;

  CHECK(MessageParser::parse(reader, messages));
  CHECK_EQ(messages.size(), 1u);
  CHECK_EQ(messages[0].type, 0x0042);
  CHECK(messages[0].payload == (std::vector<std::uint8_t>{1, 2, 3}));
}

TEST(MessageParser, ParsesMessageWithoutPayload) {
  const auto body = frame(systemMessage::Disconnect, {});
  PacketReader reader(body);
  std::vector<ParsedMessage> messages;

  CHECK(MessageParser::parse(reader, messages));
  CHECK_EQ(messages.size(), 1u);
  CHECK_EQ(messages[0].type, systemMessage::Disconnect);
  CHECK(messages[0].payload.empty());
}

TEST(MessageParser, ParsesSeveralMessagesInOrder) {
  auto body = frame(0x0001, {0xAA});
  const auto second = frame(0x0002, {0xBB, 0xCC});
  const auto third = frame(0x0003, {});
  body.insert(body.end(), second.begin(), second.end());
  body.insert(body.end(), third.begin(), third.end());
  PacketReader reader(body);
  std::vector<ParsedMessage> messages;

  CHECK(MessageParser::parse(reader, messages));
  CHECK_EQ(messages.size(), 3u);
  CHECK_EQ(messages[0].type, 0x0001);
  CHECK_EQ(messages[1].type, 0x0002);
  CHECK_EQ(messages[2].type, 0x0003);
  CHECK(messages[1].payload == (std::vector<std::uint8_t>{0xBB, 0xCC}));
}

TEST(MessageParser, ConsumesTheWholeBody) {
  auto body = frame(0x0010, noise(10));
  const auto second = frame(0x0011, noise(20, 2));
  body.insert(body.end(), second.begin(), second.end());
  PacketReader reader(body);
  std::vector<ParsedMessage> messages;

  CHECK(MessageParser::parse(reader, messages));
  CHECK(reader.isEnd());
}

TEST(MessageParser, KeepsPayloadBytes) {
  const auto payload = noise(200, 7);
  const auto body = frame(0x0099, payload);
  PacketReader reader(body);
  std::vector<ParsedMessage> messages;

  CHECK(MessageParser::parse(reader, messages));
  CHECK(messages[0].payload == payload);
}

TEST(MessageParser, RejectsTruncatedHeader) {
  const std::vector<std::uint8_t> body = {0x00, 0x01, 0x00};
  PacketReader reader(body);
  std::vector<ParsedMessage> messages;

  CHECK_FALSE(MessageParser::parse(reader, messages));
}

TEST(MessageParser, RejectsSizeLargerThanBody) {
  const std::vector<std::uint8_t> body = {0x00, 0x01, 0x00, 0x05, 1, 2};
  PacketReader reader(body);
  std::vector<ParsedMessage> messages;

  CHECK_FALSE(MessageParser::parse(reader, messages));
}

TEST(MessageParser, RejectsTrailingGarbage) {
  auto body = frame(0x0001, {1});
  body.push_back(0xFF);
  PacketReader reader(body);
  std::vector<ParsedMessage> messages;

  CHECK_FALSE(MessageParser::parse(reader, messages));
}

TEST(MessageParser, FailureLeavesOutputUntouched) {
  std::vector<ParsedMessage> messages(2);
  messages[0].type = 0x7777;
  auto body = frame(0x0001, {1});
  body.push_back(0x00);
  PacketReader reader(body);

  CHECK_FALSE(MessageParser::parse(reader, messages));
  CHECK_EQ(messages.size(), 2u);
  CHECK_EQ(messages[0].type, 0x7777);
}

TEST(MessageParser, ReplacesPreviousOutput) {
  std::vector<ParsedMessage> messages(3);
  const auto body = frame(0x0005, {9});
  PacketReader reader(body);

  CHECK(MessageParser::parse(reader, messages));
  CHECK_EQ(messages.size(), 1u);
  CHECK_EQ(messages[0].type, 0x0005);
}

TEST(MessageParser, StartsAtReaderOffset) {
  std::vector<std::uint8_t> body = {0xDE, 0xAD};
  const auto message = frame(0x0008, {4});
  body.insert(body.end(), message.begin(), message.end());
  PacketReader reader(body);
  std::uint16_t skipped = 0;
  reader.readUInt16(skipped);
  std::vector<ParsedMessage> messages;

  CHECK(MessageParser::parse(reader, messages));
  CHECK_EQ(messages.size(), 1u);
  CHECK_EQ(messages[0].type, 0x0008);
}

TEST(MessageParser, ParsesBodyAfterPacketHeader) {
  const auto packet = rtype::test::datagram(
      rtype::test::peerHeader(5), frame(systemMessage::Ping, {0, 0, 0, 1}));
  PacketReader reader(packet);
  PacketHeader header;
  std::vector<ParsedMessage> messages;

  CHECK(header.deserialize(reader));
  CHECK(MessageParser::parse(reader, messages));
  CHECK_EQ(messages.size(), 1u);
  CHECK_EQ(messages[0].type, systemMessage::Ping);
}

TEST(MessageParser, ParsesAFullSizeBody) {
  const auto payload = noise(MAX_MESSAGE_SIZE - MessageHeader::HEADER_SIZE);
  const auto body = frame(0x0050, payload);
  PacketReader reader(body);
  std::vector<ParsedMessage> messages;

  CHECK_EQ(body.size(), MAX_MESSAGE_SIZE);
  CHECK(MessageParser::parse(reader, messages));
  CHECK_EQ(messages[0].payload.size(), payload.size());
}

TEST(MessageParser, ParsesManySmallMessages) {
  std::vector<std::uint8_t> body;
  for (std::uint16_t i = 0; i < 50; ++i) {
    const auto message = frame(i, {static_cast<std::uint8_t>(i)});
    body.insert(body.end(), message.begin(), message.end());
  }
  PacketReader reader(body);
  std::vector<ParsedMessage> messages;

  CHECK(MessageParser::parse(reader, messages));
  CHECK_EQ(messages.size(), 50u);
  for (std::uint16_t i = 0; i < 50; ++i) {
    CHECK_EQ(messages[i].type, i);
    CHECK_EQ(messages[i].payload[0], i);
  }
}

// Ping and Pong

TEST(PingMessage, SizeIsFourBytes) {
  PingMessage ping;
  ping.timestamp = 1;
  PacketWriter writer;

  CHECK(ping.serialize(writer));
  CHECK_EQ(writer.getSize(), PingMessage::SIZE);
  CHECK_EQ(PingMessage::SIZE, 4u);
}

TEST(PingMessage, RoundTrips) {
  PingMessage ping;
  ping.timestamp = 0x89ABCDEF;
  PacketWriter writer;
  ping.serialize(writer);

  PacketReader reader(writer.getData());
  PingMessage decoded;
  CHECK(decoded.deserialize(reader));
  CHECK_EQ(decoded.timestamp, 0x89ABCDEFu);
}

TEST(PingMessage, TimestampIsBigEndian) {
  PingMessage ping;
  ping.timestamp = 0x00000102;
  PacketWriter writer;
  ping.serialize(writer);

  CHECK(writer.getData() == (std::vector<std::uint8_t>{0, 0, 1, 2}));
}

TEST(PingMessage, RejectsShortPayload) {
  const std::vector<std::uint8_t> bytes = {0, 0, 1};
  PacketReader reader(bytes);
  PingMessage decoded;

  CHECK_FALSE(decoded.deserialize(reader));
}

TEST(PongMessage, SizeIsFourBytes) {
  PongMessage pong;
  PacketWriter writer;

  CHECK(pong.serialize(writer));
  CHECK_EQ(writer.getSize(), PongMessage::SIZE);
  CHECK_EQ(PongMessage::SIZE, 4u);
}

TEST(PongMessage, RoundTrips) {
  PongMessage pong;
  pong.timestamp = 123456789;
  PacketWriter writer;
  pong.serialize(writer);

  PacketReader reader(writer.getData());
  PongMessage decoded;
  CHECK(decoded.deserialize(reader));
  CHECK_EQ(decoded.timestamp, 123456789u);
}

TEST(PongMessage, RejectsEmptyPayload) {
  const std::vector<std::uint8_t> bytes;
  PacketReader reader(bytes);
  PongMessage decoded;

  CHECK_FALSE(decoded.deserialize(reader));
}

TEST(PongMessage, EchoesPingEncoding) {
  PingMessage ping;
  ping.timestamp = 0xCAFEBABE;
  PongMessage pong;
  pong.timestamp = ping.timestamp;
  PacketWriter pingWriter;
  PacketWriter pongWriter;

  ping.serialize(pingWriter);
  pong.serialize(pongWriter);
  CHECK(pingWriter.getData() == pongWriter.getData());
}

// System messages and configuration

TEST(SystemMessages, IdsMatchTheProtocol) {
  CHECK_EQ(systemMessage::Ping, 0x03);
  CHECK_EQ(systemMessage::Disconnect, 0x04);
  CHECK_EQ(systemMessage::Pong, 0x86);
}

TEST(SystemMessages, ServerToClientIdsHaveHighBit) {
  CHECK((systemMessage::Pong & 0x80) != 0);
  CHECK((systemMessage::Ping & 0x80) == 0);
  CHECK((systemMessage::Disconnect & 0x80) == 0);
}

TEST(NetworkConfig, PayloadFitsTheSafeDatagram) {
  CHECK_EQ(MAX_PAYLOAD_SIZE,
           MAX_PACKET_SIZE - IP_HEADER_SIZE - UDP_HEADER_SIZE);
  CHECK_EQ(MAX_PAYLOAD_SIZE, 548u);
}

TEST(NetworkConfig, MessagesFitAfterThePacketHeader) {
  CHECK_EQ(MAX_MESSAGE_SIZE, MAX_PAYLOAD_SIZE - PACKET_HEADER_SIZE);
  CHECK_EQ(MAX_MESSAGE_SIZE, 529u);
}

TEST(NetworkConfig, TimingsAreConsistent) {
  CHECK(FLUSH_INTERVAL < ACK_ONLY_DELAY);
  CHECK(ACK_ONLY_DELAY < RESEND_TIMEOUT);
  CHECK(KEEPALIVE_INTERVAL < CONNECTION_TIMEOUT);
}

TEST(NetworkConfig, LimitsArePositive) {
  CHECK(MAX_CONNECTIONS > 0);
  CHECK(MAX_QUEUED_PACKETS > 0);
  CHECK(MAX_PENDING_PACKETS >= MAX_QUEUED_PACKETS);
  CHECK(RELIABLE_MAX_RESENDS > CRITICAL_RESENDS_BEFORE_LOSS);
  CHECK(MAX_CRITICAL_LOSSES > 0);
}
