#include "Test.hpp"

#include <array>
#include <cstdint>
#include <vector>

#include "protocol/PacketReader.hpp"

TEST(PacketReader, StartsAtOffsetZero) {
  const std::vector<std::uint8_t> data = {1, 2, 3};
  PacketReader reader(data);

  CHECK_EQ(reader.getOffset(), 0u);
  CHECK_EQ(reader.getRemaining(), 3u);
  CHECK_FALSE(reader.isEnd());
}

TEST(PacketReader, EmptyBufferIsAtEnd) {
  const std::vector<std::uint8_t> data;
  PacketReader reader(data);

  CHECK(reader.isEnd());
  CHECK_EQ(reader.getRemaining(), 0u);
}

TEST(PacketReader, ReadsUInt8) {
  const std::vector<std::uint8_t> data = {0xAB};
  PacketReader reader(data);
  std::uint8_t value = 0;

  CHECK(reader.readUInt8(value));
  CHECK_EQ(value, 0xAB);
  CHECK(reader.isEnd());
}

TEST(PacketReader, ReadsUInt16BigEndian) {
  const std::vector<std::uint8_t> data = {0x12, 0x34};
  PacketReader reader(data);
  std::uint16_t value = 0;

  CHECK(reader.readUInt16(value));
  CHECK_EQ(value, 0x1234);
}

TEST(PacketReader, ReadsUInt32BigEndian) {
  const std::vector<std::uint8_t> data = {0xDE, 0xAD, 0xBE, 0xEF};
  PacketReader reader(data);
  std::uint32_t value = 0;

  CHECK(reader.readUInt32(value));
  CHECK_EQ(value, 0xDEADBEEFu);
}

TEST(PacketReader, ReadsMaxValues) {
  const std::vector<std::uint8_t> data(7, 0xFF);
  PacketReader reader(data);
  std::uint8_t byte = 0;
  std::uint16_t word = 0;
  std::uint32_t dword = 0;

  CHECK(reader.readUInt8(byte));
  CHECK(reader.readUInt16(word));
  CHECK(reader.readUInt32(dword));
  CHECK_EQ(byte, 0xFF);
  CHECK_EQ(word, 0xFFFF);
  CHECK_EQ(dword, 0xFFFFFFFFu);
}

TEST(PacketReader, ReadsSequentialValues) {
  const std::vector<std::uint8_t> data = {0x01, 0x00, 0x02, 0x00,
                                          0x00, 0x00, 0x03};
  PacketReader reader(data);
  std::uint8_t a = 0;
  std::uint16_t b = 0;
  std::uint32_t c = 0;

  CHECK(reader.readUInt8(a));
  CHECK(reader.readUInt16(b));
  CHECK(reader.readUInt32(c));
  CHECK_EQ(a, 1);
  CHECK_EQ(b, 2);
  CHECK_EQ(c, 3u);
  CHECK(reader.isEnd());
}

TEST(PacketReader, OffsetAdvancesBySizeRead) {
  const std::vector<std::uint8_t> data(7, 0);
  PacketReader reader(data);
  std::uint8_t a = 0;
  std::uint16_t b = 0;
  std::uint32_t c = 0;

  reader.readUInt8(a);
  CHECK_EQ(reader.getOffset(), 1u);
  reader.readUInt16(b);
  CHECK_EQ(reader.getOffset(), 3u);
  reader.readUInt32(c);
  CHECK_EQ(reader.getOffset(), 7u);
}

TEST(PacketReader, RemainingDecreases) {
  const std::vector<std::uint8_t> data(5, 0);
  PacketReader reader(data);
  std::uint16_t value = 0;

  reader.readUInt16(value);
  CHECK_EQ(reader.getRemaining(), 3u);
}

TEST(PacketReader, UInt8FailsOnEmpty) {
  const std::vector<std::uint8_t> data;
  PacketReader reader(data);
  std::uint8_t value = 42;

  CHECK_FALSE(reader.readUInt8(value));
  CHECK_EQ(value, 42);
}

TEST(PacketReader, UInt16FailsWithOneByte) {
  const std::vector<std::uint8_t> data = {0x12};
  PacketReader reader(data);
  std::uint16_t value = 7;

  CHECK_FALSE(reader.readUInt16(value));
  CHECK_EQ(value, 7);
  CHECK_EQ(reader.getOffset(), 0u);
}

TEST(PacketReader, UInt32FailsWithThreeBytes) {
  const std::vector<std::uint8_t> data = {1, 2, 3};
  PacketReader reader(data);
  std::uint32_t value = 9;

  CHECK_FALSE(reader.readUInt32(value));
  CHECK_EQ(value, 9u);
  CHECK_EQ(reader.getOffset(), 0u);
}

TEST(PacketReader, FailedReadKeepsOffset) {
  const std::vector<std::uint8_t> data = {1, 2, 3};
  PacketReader reader(data);
  std::uint16_t word = 0;
  std::uint32_t dword = 0;

  CHECK(reader.readUInt16(word));
  CHECK_FALSE(reader.readUInt32(dword));
  CHECK_EQ(reader.getOffset(), 2u);
  CHECK_EQ(reader.getRemaining(), 1u);
}

TEST(PacketReader, ReadsBytes) {
  const std::vector<std::uint8_t> data = {9, 8, 7, 6};
  PacketReader reader(data);
  std::array<std::uint8_t, 4> out{};

  CHECK(reader.readBytes(out.data(), out.size()));
  CHECK(out == (std::array<std::uint8_t, 4>{9, 8, 7, 6}));
  CHECK(reader.isEnd());
}

TEST(PacketReader, ReadBytesTooLongFails) {
  const std::vector<std::uint8_t> data = {1, 2};
  PacketReader reader(data);
  std::array<std::uint8_t, 3> out{};

  CHECK_FALSE(reader.readBytes(out.data(), out.size()));
  CHECK_EQ(reader.getOffset(), 0u);
}

TEST(PacketReader, ReadZeroBytesSucceeds) {
  const std::vector<std::uint8_t> data;
  PacketReader reader(data);
  std::uint8_t out = 0;

  CHECK(reader.readBytes(&out, 0));
  CHECK(reader.readBytes(nullptr, 0));
}

TEST(PacketReader, ReadBytesNullDestinationFails) {
  const std::vector<std::uint8_t> data = {1, 2};
  PacketReader reader(data);

  CHECK_FALSE(reader.readBytes(nullptr, 2));
  CHECK_EQ(reader.getOffset(), 0u);
}

TEST(PacketReader, ResetGoesBackToStart) {
  const std::vector<std::uint8_t> data = {0x00, 0x05};
  PacketReader reader(data);
  std::uint16_t first = 0;
  std::uint16_t second = 0;

  CHECK(reader.readUInt16(first));
  CHECK(reader.isEnd());
  reader.reset();
  CHECK_EQ(reader.getOffset(), 0u);
  CHECK(reader.readUInt16(second));
  CHECK_EQ(first, second);
}

TEST(PacketReader, PointerConstructorReadsRawBuffer) {
  const std::uint8_t raw[] = {0xCA, 0xFE};
  PacketReader reader(raw, sizeof(raw));
  std::uint16_t value = 0;

  CHECK(reader.readUInt16(value));
  CHECK_EQ(value, 0xCAFE);
}

TEST(PacketReader, PointerConstructorHonorsSize) {
  const std::uint8_t raw[] = {0x01, 0x02, 0x03, 0x04};
  PacketReader reader(raw, 2);
  std::uint32_t value = 0;

  CHECK_EQ(reader.getRemaining(), 2u);
  CHECK_FALSE(reader.readUInt32(value));
}

TEST(PacketReader, ReadsExactlyTheWholeBuffer) {
  const std::vector<std::uint8_t> data = {0, 0, 0, 1};
  PacketReader reader(data);
  std::uint32_t value = 0;
  std::uint8_t extra = 0;

  CHECK(reader.readUInt32(value));
  CHECK(reader.isEnd());
  CHECK_FALSE(reader.readUInt8(extra));
}

TEST(PacketReader, DoesNotCopyTheBuffer) {
  std::vector<std::uint8_t> data = {0x00};
  PacketReader reader(data);
  data[0] = 0x55;
  std::uint8_t value = 0;

  CHECK(reader.readUInt8(value));
  CHECK_EQ(value, 0x55);
}
