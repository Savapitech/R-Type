#pragma once 

#include "Protocol/MessageType.hpp"
#include "config/NetworkConfig.hpp"
#include "protocol/PacketReader.hpp"
#include "protocol/PacketWriter.hpp"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace rtype::protocol {

using NetworkEntityId = std::uint32_t;

enum class BlueprintId : std::uint16_t {
  Player = 1,
  Missile = 2,
  EnemyBidos = 3,
  Gate = 4,
  Ramp = 5,
  Reactor = 6,
  HighGround = 7,
  LowGround = 8,
  MediumGround = 9
};

inline bool isValidBlueprint(BlueprintId blueprint) {
  const auto id = static_cast<std::uint16_t>(blueprint);
  return id >= 1 && id <= 9;
}

inline constexpr float POSITION_SCALE = 4.0f;
inline constexpr float POSITION_OFFSET = 8192.0f;
inline constexpr float MIN_POSITION = -POSITION_OFFSET;
inline constexpr float MAX_POSITION = static_cast<float>(std::numeric_limits<std::uint16_t>::max()) / POSITION_SCALE - POSITION_OFFSET;

inline bool encodePosition(float position, std::uint16_t &encoded) {
  if (!std::isfinite(position) || position < MIN_POSITION || position > MAX_POSITION)
    return false;

  const float scaled = (position + POSITION_OFFSET) * POSITION_SCALE;
  const long rounded = std::lround(scaled);

  if (rounded < 0 || rounded > std::numeric_limits<std::uint16_t>::max())
    return false;

  encoded = static_cast<std::uint16_t>(rounded);
  return true;
}

inline float decodePosition(std::uint16_t encoded) {
  return static_cast<float>(encoded) / POSITION_SCALE - POSITION_OFFSET;
}

struct CreateEntityPayload {
  static constexpr std::size_t SIZE = 10;

  NetworkEntityId entityId = 0;
  BlueprintId blueprintId = BlueprintId::Player;
  float x = 0.0f;
  float y = 0.0f;

  bool serialize(PacketWriter&writer) const {
    if (entityId == 0 || !isValidBlueprint(blueprintId))
      return false;

    std::uint16_t encodedX = 0;
    std::uint16_t encodedY = 0;
    if (!encodePosition(x, encodedX) || !encodePosition(y, encodedY))
      return false;

    if (writer.getSize() > MAX_PAYLOAD_SIZE || SIZE > MAX_PAYLOAD_SIZE - writer.getSize())
      return false;

    return writer.writeUInt32(entityId) && writer.writeUInt16(static_cast<std::uint16_t>(blueprintId)) &&
        writer.writeUInt16(encodedX) && writer.writeUInt16(encodedY);
  }

  bool deserialize(PacketReader &reader) {
    std::uint32_t id = 0;
    std::uint16_t blueprint = 0;
    std::uint16_t encodedX = 0;
    std::uint16_t encodedY = 0;

    if (!reader.readUInt32(id) || !reader.readUInt16(blueprint) || !reader.readUInt16(encodedX) || !reader.readUInt16(encodedY))
      return false;

    const auto decodedBluep = static_cast<BlueprintId>(blueprint);

    if (id == 0 || !isValidBlueprint(decodedBluep))
      return false;

    entityId = id;
    blueprintId = decodedBluep;
    x = decodePosition(encodedX);
    y = decodePosition(encodedY);
    return true;
  }
};

struct DestroyEntityPayload {
  static constexpr std::size_t SIZE = 4;

  NetworkEntityId entityId = 0;

  bool serialize(PacketWriter &writer) {
    if (entityId == 0)
      return false;

    if (writer.getSize() > MAX_PAYLOAD_SIZE || SIZE > MAX_PAYLOAD_SIZE - writer.getSize())
      return false;

    return writer.writeUInt32(entityId);
  }

  bool deserialize(PacketReader &reader) {
    std::uint32_t id =0;

    if (!reader.readUInt32(id) || id == 0)
      return false;

    entityId = id;
    return true;
  }
};
} //namespace rtype:protocol
