#include "Test.hpp"

#include <cstdint>
#include <vector>

#include "config/NetworkConfig.hpp"
#include "protocol/HandshakeHeader.hpp"
#include "protocol/MessageHeader.hpp"
#include "protocol/PacketHeader.hpp"
#include "protocol/PacketReader.hpp"
#include "protocol/PacketWriter.hpp"

namespace {

PacketHeader sampleHeader() {
  PacketHeader header;
  header.connectionId = 0x01020304;
  header.sequence = 0x0A0B0C0D;
  header.ack = 0x11223344;
  header.ackBits = 0x80000001;
  header.priority = PacketPriority::Reliable;
  header.reliableId = 0xBEEF;
  return header;
}

std::vector<std::uint8_t> serialized(const PacketHeader &header) {
  PacketWriter writer;
  header.serialize(writer);
  return writer.getData();
}

std::vector<std::uint8_t> serialized(const HandshakeHeader &header) {
  PacketWriter writer;
  header.serialize(writer);
  return writer.getData();
}

} // namespace

// PacketHeader

TEST(PacketHeader, SizeMatchesConfig) {
  CHECK_EQ(PacketHeader::HEADER_SIZE, PACKET_HEADER_SIZE);
  CHECK_EQ(PacketHeader::HEADER_SIZE, 19u);
}

TEST(PacketHeader, DefaultsAreUnreliableAndZero) {
  const PacketHeader header;

  CHECK_EQ(header.connectionId, 0u);
  CHECK_EQ(header.sequence, 0u);
  CHECK_EQ(header.ack, 0u);
  CHECK_EQ(header.ackBits, 0u);
  CHECK_EQ(header.priority, PacketPriority::Unreliable);
  CHECK_EQ(header.reliableId, 0);
}

TEST(PacketHeader, SerializesNineteenBytes) {
  const auto bytes = serialized(sampleHeader());

  CHECK_EQ(bytes.size(), PacketHeader::HEADER_SIZE);
}

TEST(PacketHeader, SerializesFieldsInBigEndianOrder) {
  const auto bytes = serialized(sampleHeader());
  const std::vector<std::uint8_t> expected = {
      0x01, 0x02, 0x03, 0x04, 0x0A, 0x0B, 0x0C, 0x0D, 0x11, 0x22,
      0x33, 0x44, 0x80, 0x00, 0x00, 0x01, 0x01, 0xBE, 0xEF};

  CHECK(bytes == expected);
}

TEST(PacketHeader, RoundTrips) {
  const PacketHeader original = sampleHeader();
  const auto bytes = serialized(original);
  PacketReader reader(bytes);
  PacketHeader decoded;

  CHECK(decoded.deserialize(reader));
  CHECK_EQ(decoded.connectionId, original.connectionId);
  CHECK_EQ(decoded.sequence, original.sequence);
  CHECK_EQ(decoded.ack, original.ack);
  CHECK_EQ(decoded.ackBits, original.ackBits);
  CHECK_EQ(decoded.priority, original.priority);
  CHECK_EQ(decoded.reliableId, original.reliableId);
  CHECK(reader.isEnd());
}

TEST(PacketHeader, RoundTripsEveryPriority) {
  for (auto priority : {PacketPriority::Critical, PacketPriority::Reliable,
                        PacketPriority::Unreliable}) {
    PacketHeader header;
    header.priority = priority;
    const auto bytes = serialized(header);
    PacketReader reader(bytes);
    PacketHeader decoded;

    CHECK(decoded.deserialize(reader));
    CHECK_EQ(decoded.priority, priority);
  }
}

TEST(PacketHeader, PriorityIsByteSixteen) {
  PacketHeader header;
  header.priority = PacketPriority::Critical;
  CHECK_EQ(serialized(header)[16], 0);
  header.priority = PacketPriority::Unreliable;
  CHECK_EQ(serialized(header)[16], 2);
}

TEST(PacketHeader, RejectsUnknownPriority) {
  auto bytes = serialized(sampleHeader());
  bytes[16] = 3;
  PacketReader reader(bytes);
  PacketHeader decoded;

  CHECK_FALSE(decoded.deserialize(reader));
}

TEST(PacketHeader, RejectsPriorityFF) {
  auto bytes = serialized(sampleHeader());
  bytes[16] = 0xFF;
  PacketReader reader(bytes);
  PacketHeader decoded;

  CHECK_FALSE(decoded.deserialize(reader));
}

TEST(PacketHeader, RejectedHeaderIsLeftUntouched) {
  auto bytes = serialized(sampleHeader());
  bytes[16] = 9;
  PacketReader reader(bytes);
  PacketHeader decoded;
  decoded.sequence = 77;

  CHECK_FALSE(decoded.deserialize(reader));
  CHECK_EQ(decoded.sequence, 77u);
}

TEST(PacketHeader, RejectsTruncatedHeader) {
  auto bytes = serialized(sampleHeader());
  bytes.pop_back();
  PacketReader reader(bytes);
  PacketHeader decoded;

  CHECK_FALSE(decoded.deserialize(reader));
  CHECK_EQ(reader.getOffset(), 0u);
}

TEST(PacketHeader, RejectsEmptyBuffer) {
  const std::vector<std::uint8_t> bytes;
  PacketReader reader(bytes);
  PacketHeader decoded;

  CHECK_FALSE(decoded.deserialize(reader));
}

TEST(PacketHeader, LeavesBodyInReader) {
  auto bytes = serialized(sampleHeader());
  bytes.push_back(0xAA);
  bytes.push_back(0xBB);
  PacketReader reader(bytes);
  PacketHeader decoded;

  CHECK(decoded.deserialize(reader));
  CHECK_EQ(reader.getRemaining(), 2u);
  std::uint8_t first = 0;
  CHECK(reader.readUInt8(first));
  CHECK_EQ(first, 0xAA);
}

TEST(PacketHeader, SerializeFailsInFullWriter) {
  PacketWriter writer;
  const std::vector<std::uint8_t> filler(MAX_PAYLOAD_SIZE - 10, 0);
  writer.writeBytes(filler.data(), filler.size());

  CHECK_FALSE(sampleHeader().serialize(writer));
}

TEST(PacketHeader, RoundTripsMaxValues) {
  PacketHeader header;
  header.connectionId = 0xFFFFFFFF;
  header.sequence = 0xFFFFFFFF;
  header.ack = 0xFFFFFFFF;
  header.ackBits = 0xFFFFFFFF;
  header.reliableId = 0xFFFF;
  const auto bytes = serialized(header);
  PacketReader reader(bytes);
  PacketHeader decoded;

  CHECK(decoded.deserialize(reader));
  CHECK_EQ(decoded.connectionId, 0xFFFFFFFFu);
  CHECK_EQ(decoded.sequence, 0xFFFFFFFFu);
  CHECK_EQ(decoded.ack, 0xFFFFFFFFu);
  CHECK_EQ(decoded.ackBits, 0xFFFFFFFFu);
  CHECK_EQ(decoded.reliableId, 0xFFFF);
}

// HandshakeHeader

TEST(HandshakeHeader, MagicSpellsRTYP) {
  CHECK_EQ(MAGIC, 0x52545950u);
  const auto bytes = serialized(HandshakeHeader{});
  CHECK_EQ(bytes[0], 'R');
  CHECK_EQ(bytes[1], 'T');
  CHECK_EQ(bytes[2], 'Y');
  CHECK_EQ(bytes[3], 'P');
}

TEST(HandshakeHeader, DefaultsToHelloWithCurrentVersion) {
  const HandshakeHeader header;

  CHECK_EQ(header.magic, MAGIC);
  CHECK_EQ(header.version, VERSION);
  CHECK_EQ(header.type, HandshakeType::Hello);
}

TEST(HandshakeHeader, SerializesSixBytes) {
  const auto bytes = serialized(HandshakeHeader{});

  CHECK_EQ(bytes.size(), HandshakeHeader::HEADER_SIZE);
  CHECK_EQ(bytes.size(), 6u);
  CHECK_EQ(bytes[4], VERSION);
  CHECK_EQ(bytes[5], static_cast<std::uint8_t>(HandshakeType::Hello));
}

TEST(HandshakeHeader, RoundTripsEveryType) {
  for (auto type : {HandshakeType::Hello, HandshakeType::Welcome,
                    HandshakeType::Rejected}) {
    HandshakeHeader header;
    header.type = type;
    const auto bytes = serialized(header);
    PacketReader reader(bytes);
    HandshakeHeader decoded;

    CHECK(decoded.deserialize(reader));
    CHECK_EQ(decoded.type, type);
    CHECK_EQ(decoded.version, VERSION);
  }
}

TEST(HandshakeHeader, RejectsBadMagic) {
  auto bytes = serialized(HandshakeHeader{});
  bytes[0] = 'X';
  PacketReader reader(bytes);
  HandshakeHeader decoded;

  CHECK_FALSE(decoded.deserialize(reader));
}

TEST(HandshakeHeader, RejectsTypeZero) {
  auto bytes = serialized(HandshakeHeader{});
  bytes[5] = 0;
  PacketReader reader(bytes);
  HandshakeHeader decoded;

  CHECK_FALSE(decoded.deserialize(reader));
}

TEST(HandshakeHeader, RejectsTypeAfterRejected) {
  auto bytes = serialized(HandshakeHeader{});
  bytes[5] = static_cast<std::uint8_t>(HandshakeType::Rejected) + 1;
  PacketReader reader(bytes);
  HandshakeHeader decoded;

  CHECK_FALSE(decoded.deserialize(reader));
}

TEST(HandshakeHeader, RejectsTruncatedHeader) {
  auto bytes = serialized(HandshakeHeader{});
  bytes.resize(5);
  PacketReader reader(bytes);
  HandshakeHeader decoded;

  CHECK_FALSE(decoded.deserialize(reader));
}

TEST(HandshakeHeader, KeepsAnyVersionForTheCaller) {
  auto bytes = serialized(HandshakeHeader{});
  bytes[4] = 0xEE;
  PacketReader reader(bytes);
  HandshakeHeader decoded;

  CHECK(decoded.deserialize(reader));
  CHECK_EQ(decoded.version, 0xEE);
}

TEST(HandshakeHeader, WelcomeCarriesConnectionIdAfterHeader) {
  PacketWriter writer;
  HandshakeHeader header;
  header.type = HandshakeType::Welcome;
  CHECK(header.serialize(writer));
  CHECK(writer.writeUInt32(42));

  PacketReader reader(writer.getData());
  HandshakeHeader decoded;
  std::uint32_t connectionId = 0;
  CHECK(decoded.deserialize(reader));
  CHECK(reader.readUInt32(connectionId));
  CHECK_EQ(decoded.type, HandshakeType::Welcome);
  CHECK_EQ(connectionId, 42u);
  CHECK(reader.isEnd());
}

TEST(HandshakeHeader, IsShorterThanAPacketHeader) {
  CHECK(HandshakeHeader::HEADER_SIZE < PacketHeader::HEADER_SIZE);
}

// MessageHeader

TEST(MessageHeader, DefaultsToZero) {
  const MessageHeader header;

  CHECK_EQ(header.getType(), 0);
  CHECK_EQ(header.size, 0);
  CHECK_EQ(MessageHeader::HEADER_SIZE, 4u);
}

TEST(MessageHeader, SetTypeIsReturnedByGetType) {
  MessageHeader header;

  header.setType(0x0086);
  CHECK_EQ(header.getType(), 0x0086);
}

TEST(MessageHeader, SerializesTypeThenSize) {
  MessageHeader header;
  header.setType(0x1234);
  header.size = 9;
  PacketWriter writer;

  CHECK(header.serialize(writer));
  CHECK(writer.getData() ==
        (std::vector<std::uint8_t>{0x12, 0x34, 0x00, 0x09}));
}

TEST(MessageHeader, RoundTrips) {
  MessageHeader header;
  header.setType(0xFFFE);
  header.size = 0x01FF;
  PacketWriter writer;
  header.serialize(writer);

  PacketReader reader(writer.getData());
  MessageHeader decoded;
  CHECK(decoded.deserialize(reader));
  CHECK_EQ(decoded.getType(), 0xFFFE);
  CHECK_EQ(decoded.size, 0x01FF);
  CHECK(reader.isEnd());
}

TEST(MessageHeader, RoundTripsZeroPayload) {
  MessageHeader header;
  header.setType(0x0004);
  header.size = 0;
  PacketWriter writer;
  header.serialize(writer);

  PacketReader reader(writer.getData());
  MessageHeader decoded;
  CHECK(decoded.deserialize(reader));
  CHECK_EQ(decoded.size, 0);
}

TEST(MessageHeader, RejectsTwoBytes) {
  const std::vector<std::uint8_t> bytes = {0x00, 0x03};
  PacketReader reader(bytes);
  MessageHeader decoded;

  CHECK_FALSE(decoded.deserialize(reader));
}

TEST(MessageHeader, RejectsEmptyBuffer) {
  const std::vector<std::uint8_t> bytes;
  PacketReader reader(bytes);
  MessageHeader decoded;

  CHECK_FALSE(decoded.deserialize(reader));
}

TEST(MessageHeader, DoesNotReadThePayload) {
  const std::vector<std::uint8_t> bytes = {0x00, 0x03, 0x00, 0x02, 0xAA, 0xBB};
  PacketReader reader(bytes);
  MessageHeader decoded;

  CHECK(decoded.deserialize(reader));
  CHECK_EQ(reader.getRemaining(), 2u);
}

TEST(MessageHeader, SerializeFailsInFullWriter) {
  PacketWriter writer;
  const std::vector<std::uint8_t> filler(MAX_PAYLOAD_SIZE - 3, 0);
  writer.writeBytes(filler.data(), filler.size());
  MessageHeader header;

  CHECK_FALSE(header.serialize(writer));
}
