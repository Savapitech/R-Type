#include "Test.hpp"

#include <cstdint>
#include <vector>

#include "Protocol/MessageType.hpp"
#include "Protocol/PayloadCodec.hpp"
#include "Protocol/PowerUpPayload.hpp"

using namespace rtype::protocol;

TEST(PowerUpTaken, IsThreeBytesBigEndian) {
  const auto payload = powerUpTakenPayload(PowerUpKind::WeaponPlasma, 8.0f);
  PacketWriter writer;

  CHECK(writePayload(writer, payload));
  CHECK(writer.getData() == (std::vector<std::uint8_t>{0x04, 0x00, 0x50}));
}

TEST(PowerUpTaken, RoundTripsTheValueInTenths) {
  const auto payload = powerUpTakenPayload(PowerUpKind::Speed, 40.25f);
  PacketWriter writer;
  CHECK(writePayload(writer, payload));

  PacketReader reader(writer.getData());
  PowerUpTakenPayload decoded{};
  CHECK(readPayload(reader, decoded));
  CHECK(isValid(decoded));
  CHECK(decoded.kind == PowerUpKind::Speed);
  CHECK_EQ(decoded.value.get(), std::uint16_t{403});
}

TEST(PowerUpTaken, ClampsInvalidValues) {
  CHECK_EQ(powerUpTakenPayload(PowerUpKind::Heal, -5.0f).value.get(),
           std::uint16_t{0});
  CHECK_EQ(powerUpTakenPayload(PowerUpKind::Heal, 1e9f).value.get(),
           std::uint16_t{65535});
}

TEST(PowerUpTaken, RejectsUnknownKinds) {
  const std::vector<std::uint8_t> bytes = {0x05, 0x00, 0x10};
  PacketReader reader(bytes);
  PowerUpTakenPayload decoded{};

  CHECK(readPayload(reader, decoded));
  CHECK_FALSE(isValid(decoded));
}

TEST(PowerUpTaken, IsSentReliably) {
  CHECK(messagePriority(MessageType::PowerUpTaken) ==
        PacketPriority::Reliable);
}
