#include "Test.hpp"

#include <cstdint>
#include <vector>

#include "config/NetworkConfig.hpp"
#include "protocol/PacketReader.hpp"
#include "protocol/PacketWriter.hpp"

TEST(PacketWriter, StartsEmpty) {
  PacketWriter writer;

  CHECK(writer.empty());
  CHECK_EQ(writer.getSize(), 0u);
  CHECK(writer.getData().empty());
}

TEST(PacketWriter, WritesUInt8) {
  PacketWriter writer;

  CHECK(writer.writeUInt8(0x7F));
  CHECK(writer.getData() == std::vector<std::uint8_t>{0x7F});
}

TEST(PacketWriter, WritesUInt16BigEndian) {
  PacketWriter writer;

  CHECK(writer.writeUInt16(0xABCD));
  CHECK(writer.getData() == (std::vector<std::uint8_t>{0xAB, 0xCD}));
}

TEST(PacketWriter, WritesUInt32BigEndian) {
  PacketWriter writer;

  CHECK(writer.writeUInt32(0x01020304));
  CHECK(writer.getData() == (std::vector<std::uint8_t>{1, 2, 3, 4}));
}

TEST(PacketWriter, WritesZeroValues) {
  PacketWriter writer;

  CHECK(writer.writeUInt8(0));
  CHECK(writer.writeUInt16(0));
  CHECK(writer.writeUInt32(0));
  CHECK(writer.getData() == std::vector<std::uint8_t>(7, 0));
}

TEST(PacketWriter, AppendsInOrder) {
  PacketWriter writer;

  writer.writeUInt8(0x01);
  writer.writeUInt16(0x0203);
  writer.writeUInt32(0x04050607);
  CHECK(writer.getData() == (std::vector<std::uint8_t>{1, 2, 3, 4, 5, 6, 7}));
}

TEST(PacketWriter, SizeTracksWrites) {
  PacketWriter writer;

  writer.writeUInt8(1);
  CHECK_EQ(writer.getSize(), 1u);
  writer.writeUInt16(1);
  CHECK_EQ(writer.getSize(), 3u);
  writer.writeUInt32(1);
  CHECK_EQ(writer.getSize(), 7u);
  CHECK_FALSE(writer.empty());
}

TEST(PacketWriter, WritesBytes) {
  PacketWriter writer;
  const std::uint8_t raw[] = {5, 6, 7};

  CHECK(writer.writeBytes(raw, sizeof(raw)));
  CHECK(writer.getData() == (std::vector<std::uint8_t>{5, 6, 7}));
}

TEST(PacketWriter, WriteZeroBytesSucceeds) {
  PacketWriter writer;

  CHECK(writer.writeBytes(nullptr, 0));
  CHECK(writer.empty());
}

TEST(PacketWriter, WriteBytesNullSourceFails) {
  PacketWriter writer;

  CHECK_FALSE(writer.writeBytes(nullptr, 4));
  CHECK(writer.empty());
}

TEST(PacketWriter, ClearEmptiesTheBuffer) {
  PacketWriter writer;

  writer.writeUInt32(0xFFFFFFFF);
  writer.clear();
  CHECK(writer.empty());
  CHECK_EQ(writer.getSize(), 0u);
  CHECK(writer.writeUInt8(1));
  CHECK_EQ(writer.getSize(), 1u);
}

TEST(PacketWriter, AcceptsExactlyMaxPayload) {
  PacketWriter writer;
  const std::vector<std::uint8_t> block(MAX_PAYLOAD_SIZE, 0x11);

  CHECK(writer.writeBytes(block.data(), block.size()));
  CHECK_EQ(writer.getSize(), MAX_PAYLOAD_SIZE);
}

TEST(PacketWriter, RefusesBytesPastMaxPayload) {
  PacketWriter writer;
  const std::vector<std::uint8_t> block(MAX_PAYLOAD_SIZE + 1, 0x11);

  CHECK_FALSE(writer.writeBytes(block.data(), block.size()));
  CHECK(writer.empty());
}

TEST(PacketWriter, RefusesUInt8WhenFull) {
  PacketWriter writer;
  const std::vector<std::uint8_t> block(MAX_PAYLOAD_SIZE, 0);

  writer.writeBytes(block.data(), block.size());
  CHECK_FALSE(writer.writeUInt8(1));
  CHECK_EQ(writer.getSize(), MAX_PAYLOAD_SIZE);
}

TEST(PacketWriter, RefusesUInt16WithOneByteLeft) {
  PacketWriter writer;
  const std::vector<std::uint8_t> block(MAX_PAYLOAD_SIZE - 1, 0);

  writer.writeBytes(block.data(), block.size());
  CHECK_FALSE(writer.writeUInt16(1));
  CHECK(writer.writeUInt8(1));
  CHECK_EQ(writer.getSize(), MAX_PAYLOAD_SIZE);
}

TEST(PacketWriter, RefusesUInt32WithThreeBytesLeft) {
  PacketWriter writer;
  const std::vector<std::uint8_t> block(MAX_PAYLOAD_SIZE - 3, 0);

  writer.writeBytes(block.data(), block.size());
  CHECK_FALSE(writer.writeUInt32(1));
  CHECK_EQ(writer.getSize(), MAX_PAYLOAD_SIZE - 3);
}

TEST(PacketWriter, FailedWriteKeepsPreviousData) {
  PacketWriter writer;
  const std::vector<std::uint8_t> big(MAX_PAYLOAD_SIZE, 0);

  writer.writeUInt16(0xBEEF);
  CHECK_FALSE(writer.writeBytes(big.data(), big.size()));
  CHECK(writer.getData() == (std::vector<std::uint8_t>{0xBE, 0xEF}));
}

TEST(PacketWriter, RoundTripsWithReader) {
  PacketWriter writer;

  writer.writeUInt8(0x42);
  writer.writeUInt16(0x1337);
  writer.writeUInt32(0xC0FFEE00);

  PacketReader reader(writer.getData());
  std::uint8_t a = 0;
  std::uint16_t b = 0;
  std::uint32_t c = 0;
  CHECK(reader.readUInt8(a));
  CHECK(reader.readUInt16(b));
  CHECK(reader.readUInt32(c));
  CHECK_EQ(a, 0x42);
  CHECK_EQ(b, 0x1337);
  CHECK_EQ(c, 0xC0FFEE00u);
  CHECK(reader.isEnd());
}

TEST(PacketWriter, RoundTripsManyValues) {
  PacketWriter writer;
  for (std::uint32_t i = 0; i < 100; ++i)
    CHECK(writer.writeUInt32(i * 7919u));

  PacketReader reader(writer.getData());
  for (std::uint32_t i = 0; i < 100; ++i) {
    std::uint32_t value = 0;
    CHECK(reader.readUInt32(value));
    CHECK_EQ(value, i * 7919u);
  }
  CHECK(reader.isEnd());
}

TEST(PacketWriter, GetDataReturnsSameBuffer) {
  PacketWriter writer;

  const auto &data = writer.getData();
  writer.writeUInt8(3);
  CHECK_EQ(data.size(), 1u);
  CHECK_EQ(data[0], 3);
}
