#pragma once

#include <cstdint>

#include "BigEndian.hpp"
#include "Position.hpp"

namespace rtype::protocol {

using NetworkEntityId = std::uint32_t;

enum class DestroyReason : std::uint8_t {
  Unknown = 0x00,
  Collision = 0x01,
  OutOfBounds = 0x02,
  PlayerDisconnected = 0x03,
  GameEnded = 0x04
};

struct CreateEntityPayload {
  be32 serverTick;
  be32 networkId;
  be16 entityType;
  be16 positionX;
  be16 positionY;
  be16 rotation;
};

struct DestroyEntityPayload {
  be32 serverTick;
  be32 networkId;
  DestroyReason reason = DestroyReason::Unknown;
};

struct QueryEntityPayload {
  be32 networkId;
};

static_assert(sizeof(CreateEntityPayload) ==
                  sizeof(be32) * 2 + sizeof(be16) * 4,
              "CreateEntityPayload must not contain padding");
static_assert(sizeof(DestroyEntityPayload) ==
                  sizeof(be32) * 2 + sizeof(DestroyReason),
              "DestroyEntityPayload must not contain padding");
static_assert(sizeof(QueryEntityPayload) == sizeof(be32),
              "QueryEntityPayload must not contain padding");

// The server is the only one to assign network id 0 means no entity
constexpr bool isValid(const CreateEntityPayload &payload) {
  return payload.networkId.get() != 0;
}

constexpr bool isValid(const DestroyEntityPayload &payload) {
  return payload.networkId.get() != 0 &&
         static_cast<std::uint8_t>(payload.reason) <=
             static_cast<std::uint8_t>(DestroyReason::GameEnded);
}

constexpr bool isValid(const QueryEntityPayload &payload) {
  return payload.networkId.get() != 0;
}

inline CreateEntityPayload createEntityPayload(std::uint32_t serverTick,
                                               std::uint32_t networkId,
                                               std::uint16_t entityType,
                                               float x, float y,
                                               float rotation = 0.0f) {
  return {serverTick,        networkId,         entityType,
          encodePosition(x), encodePosition(y), encodeRotation(rotation)};
}

inline DestroyEntityPayload destroyEntityPayload(std::uint32_t serverTick,
                                                 std::uint32_t networkId,
                                                 DestroyReason reason) {
  return {serverTick, networkId, reason};
}

} // namespace rtype::protocol
