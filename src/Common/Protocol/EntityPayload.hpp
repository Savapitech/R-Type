#pragma once

#include <cstdint>

#include "BigEndian.hpp"
#include "Position.hpp"

namespace rtype::protocol {
/**
 * @addtogroup api_protocol
 * @{
 */

/// Server assigned identifier of a replicated entity, 0 means no entity.
using NetworkEntityId = std::uint32_t;

/**
 * @brief Reason carried by a DestroyEntity message.
 */
enum class DestroyReason : std::uint8_t {
  Unknown = 0x00,            ///< No specific reason.
  Collision = 0x01,          ///< Removed by gameplay, such as a hit.
  OutOfBounds = 0x02,        ///< Left the playable area.
  PlayerDisconnected = 0x03, ///< Owner of the ship left the game.
  GameEnded = 0x04           ///< The game is over.
};

/**
 * @brief Payload of a CreateEntity message, 16 bytes, sent by the server to
 * spawn an entity on a client.
 *
 * | Offset | Size | Field        | Description                              |
 * | -----: | ---: | ------------ | ---------------------------------------- |
 * | 0      | 4    | `serverTick` | Server tick at sending time.             |
 * | 4      | 4    | `networkId`  | Entity id, never 0.                      |
 * | 8      | 2    | `entityType` | Archetype id of the entity blueprint.    |
 * | 10     | 2    | `positionX`  | Encoded with encodePosition().           |
 * | 12     | 2    | `positionY`  | Encoded with encodePosition().           |
 * | 14     | 2    | `rotation`   | Encoded with encodeRotation().           |
 */
struct CreateEntityPayload {
  be32 serverTick; ///< Server tick at sending time.
  be32 networkId;  ///< Identifier of the new entity.
  be16 entityType; ///< Archetype id of the entity blueprint.
  be16 positionX;  ///< Horizontal position, fixed point encoded.
  be16 positionY;  ///< Vertical position, fixed point encoded.
  be16 rotation;   ///< Rotation, fraction of a full turn.
};

/**
 * @brief Payload of a DestroyEntity message, 9 bytes, sent by the server to
 * remove an entity from a client.
 *
 * | Offset | Size | Field        | Description                    |
 * | -----: | ---: | ------------ | ------------------------------ |
 * | 0      | 4    | `serverTick` | Server tick at sending time.   |
 * | 4      | 4    | `networkId`  | Entity id, never 0.            |
 * | 8      | 1    | `reason`     | A DestroyReason value.         |
 */
struct DestroyEntityPayload {
  be32 serverTick; ///< Server tick at sending time.
  be32 networkId;  ///< Identifier of the removed entity.
  DestroyReason reason = DestroyReason::Unknown; ///< Why the entity is removed.
};

/**
 * @brief Payload of a QueryEntity message, 4 bytes, sent by a client that
 * received a snapshot for an entity it does not know.
 *
 * | Offset | Size | Field       | Description              |
 * | -----: | ---: | ----------- | ------------------------ |
 * | 0      | 4    | `networkId` | Entity id, never 0.      |
 */
struct QueryEntityPayload {
  be32 networkId; ///< Identifier of the unknown entity.
};

static_assert(sizeof(CreateEntityPayload) ==
                  sizeof(be32) * 2 + sizeof(be16) * 4,
              "CreateEntityPayload must not contain padding");
static_assert(sizeof(DestroyEntityPayload) ==
                  sizeof(be32) * 2 + sizeof(DestroyReason),
              "DestroyEntityPayload must not contain padding");
static_assert(sizeof(QueryEntityPayload) == sizeof(be32),
              "QueryEntityPayload must not contain padding");

/**
 * @brief Checks that the entity id is not 0.
 * @param payload Decoded payload.
 * @return True if the payload is valid.
 */
constexpr bool isValid(const CreateEntityPayload &payload) {
  return payload.networkId.get() != 0;
}

/**
 * @brief Checks the entity id and the destroy reason.
 * @param payload Decoded payload.
 * @return True if the payload is valid.
 */
constexpr bool isValid(const DestroyEntityPayload &payload) {
  return payload.networkId.get() != 0 &&
         static_cast<std::uint8_t>(payload.reason) <=
             static_cast<std::uint8_t>(DestroyReason::GameEnded);
}

/**
 * @brief Checks that the entity id is not 0.
 * @param payload Decoded payload.
 * @return True if the payload is valid.
 */
constexpr bool isValid(const QueryEntityPayload &payload) {
  return payload.networkId.get() != 0;
}

/**
 * @brief Builds a CreateEntity payload from world values.
 * @param serverTick Current server tick.
 * @param networkId Identifier of the entity.
 * @param entityType Archetype id of the entity blueprint.
 * @param x Horizontal position in pixels.
 * @param y Vertical position in pixels.
 * @param rotation Rotation in radians.
 * @return The encoded payload.
 */
inline CreateEntityPayload createEntityPayload(std::uint32_t serverTick,
                                               std::uint32_t networkId,
                                               std::uint16_t entityType,
                                               float x, float y,
                                               float rotation = 0.0f) {
  return {serverTick,        networkId,         entityType,
          encodePosition(x), encodePosition(y), encodeRotation(rotation)};
}

/**
 * @brief Builds a DestroyEntity payload.
 * @param serverTick Current server tick.
 * @param networkId Identifier of the entity.
 * @param reason Why the entity is removed.
 * @return The encoded payload.
 */
inline DestroyEntityPayload destroyEntityPayload(std::uint32_t serverTick,
                                                 std::uint32_t networkId,
                                                 DestroyReason reason) {
  return {serverTick, networkId, reason};
}

/** @} */
} // namespace rtype::protocol
