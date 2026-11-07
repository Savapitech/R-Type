#include "Test.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <type_traits>

#include "Protocol/BigEndian.hpp"
#include "Protocol/Position.hpp"

using namespace rtype::protocol;

constexpr float PI = std::numbers::pi_v<float>;

// BigEndian

TEST(BigEndian, ByteswapIsConstexpr) {
  static_assert(detail::byteswap<std::uint16_t>(0x1234) == 0x3412);
  static_assert(detail::byteswap<std::uint32_t>(0x11223344) == 0x44332211);
  static_assert(detail::byteswap<std::uint8_t>(0xAB) == 0xAB);
  CHECK(true);
}

TEST(BigEndian, ByteswapTwiceIsIdentity) {
  for (std::uint32_t value : {0u, 1u, 0xDEADBEEFu, 0xFFFFFFFFu, 0x00FF00FFu})
    CHECK_EQ(detail::byteswap(detail::byteswap(value)), value);
}

TEST(BigEndian, Be16StoresMostSignificantByteFirst) {
  const be16 value = 0xABCD;

  CHECK_EQ(value.bytes[0], 0xAB);
  CHECK_EQ(value.bytes[1], 0xCD);
}

TEST(BigEndian, Be32StoresMostSignificantByteFirst) {
  const be32 value = 0x01020304;

  CHECK_EQ(value.bytes[0], 0x01);
  CHECK_EQ(value.bytes[1], 0x02);
  CHECK_EQ(value.bytes[2], 0x03);
  CHECK_EQ(value.bytes[3], 0x04);
}

TEST(BigEndian, GetReturnsTheHostValue) {
  const be32 value = 123456789u;

  CHECK_EQ(value.get(), 123456789u);
  CHECK_EQ(static_cast<std::uint32_t>(value), 123456789u);
}

TEST(BigEndian, DefaultIsZero) {
  const be16 small;
  const be32 big;

  CHECK_EQ(small.get(), 0);
  CHECK_EQ(big.get(), 0u);
}

TEST(BigEndian, HasNoPadding) {
  static_assert(sizeof(be16) == 2);
  static_assert(sizeof(be32) == 4);
  static_assert(alignof(be32) == 1);
  CHECK(std::is_trivially_copyable_v<be32>);
}

TEST(BigEndian, RoundTripsExtremes) {
  CHECK_EQ(be16(std::numeric_limits<std::uint16_t>::max()).get(), 0xFFFF);
  CHECK_EQ(be32(std::numeric_limits<std::uint32_t>::max()).get(), 0xFFFFFFFFu);
  CHECK_EQ(be32(0u).get(), 0u);
}

TEST(BigEndian, IsUsableInConstantExpressions) {
  constexpr be32 value = 0x0A0B0C0D;
  static_assert(value.get() == 0x0A0B0C0D);
  static_assert(value.bytes[0] == 0x0A);
  CHECK(true);
}

// Position

TEST(Position, ZeroIsTheMiddleOfTheRange) {
  CHECK_EQ(encodePosition(0.0f).get(), 32768);
  CHECK_EQ(decodePosition(encodePosition(0.0f)), 0.0f);
}

TEST(Position, ResolutionIsAQuarterPixel) {
  CHECK_EQ(1.0f / POSITION_SCALE, 0.25f);
  CHECK_EQ(encodePosition(0.25f).get(), 32769);
  CHECK_EQ(decodePosition(encodePosition(0.25f)), 0.25f);
}

TEST(Position, RoundsToTheNearestStep) {
  CHECK_EQ(decodePosition(encodePosition(10.1f)), 10.0f);
  CHECK_EQ(decodePosition(encodePosition(10.2f)), 10.25f);
  CHECK_EQ(decodePosition(encodePosition(-3.9f)), -4.0f);
}

TEST(Position, RoundTripsScreenCoordinates) {
  for (float position = -500.0f; position <= 2500.0f; position += 12.75f)
    CHECK_EQ(decodePosition(encodePosition(position)), position);
}

TEST(Position, ErrorIsAtMostAnEighthOfPixel) {
  for (float position = -100.0f; position < 100.0f; position += 0.37f)
    CHECK_NEAR(decodePosition(encodePosition(position)), position, 0.126);
}

TEST(Position, LimitsMatchTheEncoding) {
  CHECK_EQ(MIN_POSITION, -8192.0f);
  CHECK_EQ(MAX_POSITION, 8191.75f);
  CHECK_EQ(encodePosition(MIN_POSITION).get(), 0);
  CHECK_EQ(encodePosition(MAX_POSITION).get(), 0xFFFF);
}

TEST(Position, ClampsOutOfRangeValues) {
  CHECK_EQ(encodePosition(1e9f).get(), 0xFFFF);
  CHECK_EQ(encodePosition(-1e9f).get(), 0);
  CHECK_EQ(decodePosition(encodePosition(10000.0f)), MAX_POSITION);
  CHECK_EQ(decodePosition(encodePosition(-10000.0f)), MIN_POSITION);
}

TEST(Position, NonFiniteBecomesZero) {
  CHECK_EQ(decodePosition(encodePosition(std::nanf(""))), 0.0f);
  CHECK_EQ(
      decodePosition(encodePosition(std::numeric_limits<float>::infinity())),
      0.0f);
  CHECK_EQ(
      decodePosition(encodePosition(-std::numeric_limits<float>::infinity())),
      0.0f);
}

TEST(Position, OrderIsPreserved) {
  CHECK(encodePosition(-1.0f).get() < encodePosition(0.0f).get());
  CHECK(encodePosition(0.0f).get() < encodePosition(1.0f).get());
}

// Rotation

TEST(Rotation, ZeroIsZero) {
  CHECK_EQ(encodeRotation(0.0f).get(), 0);
  CHECK_EQ(decodeRotation(be16(0)), 0.0f);
}

TEST(Rotation, HalfTurn) {
  CHECK_EQ(encodeRotation(PI).get(), 32768);
  CHECK_NEAR(decodeRotation(be16(32768)), PI, 1e-5);
}

TEST(Rotation, QuarterTurn) {
  CHECK_EQ(encodeRotation(PI / 2.0f).get(), 16384);
  CHECK_NEAR(decodeRotation(be16(16384)), PI / 2.0f, 1e-5);
}

TEST(Rotation, FullTurnWrapsToZero) {
  CHECK_EQ(encodeRotation(2.0f * PI).get(), 0);
  CHECK_EQ(encodeRotation(4.0f * PI).get(), 0);
}

TEST(Rotation, NegativeAnglesWrap) {
  CHECK_EQ(encodeRotation(-PI / 2.0f).get(), 49152);
  CHECK_EQ(encodeRotation(-PI).get(), 32768);
}

TEST(Rotation, JustBelowAFullTurnWrapsToZero) {
  CHECK_EQ(encodeRotation(2.0f * PI - 1e-6f).get(), 0);
}

TEST(Rotation, NonFiniteBecomesZero) {
  CHECK_EQ(encodeRotation(std::nanf("")).get(), 0);
  CHECK_EQ(encodeRotation(std::numeric_limits<float>::infinity()).get(), 0);
}

TEST(Rotation, RoundTripsWithSmallError) {
  for (float angle = 0.0f; angle < 2.0f * PI; angle += 0.1f)
    CHECK_NEAR(decodeRotation(encodeRotation(angle)), angle, 1e-3);
}
