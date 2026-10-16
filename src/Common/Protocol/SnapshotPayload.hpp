#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_set>
#include <utility>
#include <vector>

#include "BigEndian.hpp"
#include "EntityPayload.hpp"
#include "PayloadCodec.hpp"
#include "Position.hpp"

#include "config/NetworkConfig.hpp"
#include "protocol/MessageHeader.hpp"

namespace rtype::protocol {
/**
 * @addtogroup api_protocol
 * @{
 */

/**
 * @brief Fixed part of a Snapshot message, 6 bytes.
 *
 * | Offset | Size | Field         | Description                         |
 * | -----: | ---: | ------------- | ----------------------------------- |
 * | 0      | 4    | `serverTick`  | Server tick the states belong to.   |
 * | 4      | 2    | `entityCount` | Number of SnapshotEntityState that follow.
 * |
 */
struct SnapshotHeader {
  be32 serverTick;  ///< Server tick the states belong to.
  be16 entityCount; ///< Number of entity states that follow.
};

/**
 * @brief State of one entity inside a Snapshot message, 11 bytes.
 *
 * | Offset | Size | Field        | Description                              |
 * | -----: | ---: | ------------ | ---------------------------------------- |
 * | 0      | 4    | `networkId`  | Entity id, never 0, unique per snapshot. |
 * | 4      | 1    | `entityType` | Low 8 bits of the archetype id.          |
 * | 5      | 2    | `positionX`  | Encoded with encodePosition().           |
 * | 7      | 2    | `positionY`  | Encoded with encodePosition().           |
 * | 9      | 2    | `rotation`   | Encoded with encodeRotation().           |
 */
struct SnapshotEntityState {
  be32 networkId;              ///< Identifier of the entity.
  std::uint8_t entityType = 0; ///< Low 8 bits of the archetype id.
  be16 positionX;              ///< Horizontal position, fixed point encoded.
  be16 positionY;              ///< Vertical position, fixed point encoded.
  be16 rotation;               ///< Rotation, fraction of a full turn.
};

static_assert(sizeof(SnapshotHeader) == sizeof(be32) + sizeof(be16),
              "SnapshotHeader must not contain padding");
static_assert(sizeof(SnapshotEntityState) ==
                  sizeof(be32) + sizeof(std::uint8_t) + sizeof(be16) * 3,
              "SnapshotEntityState must not contain padding");

/// Largest number of entity states that fit in one Snapshot message.
inline constexpr std::size_t SNAPSHOT_MAX_ENTITIES =
    (MAX_MESSAGE_SIZE - MessageHeader::HEADER_SIZE - sizeof(SnapshotHeader)) /
    sizeof(SnapshotEntityState);

/**
 * @brief Decoded Snapshot message: a SnapshotHeader followed by
 * `entityCount` SnapshotEntityState records.
 */
struct SnapshotPayload {
  be32 serverTick; ///< Server tick the states belong to.
  std::vector<SnapshotEntityState> entities; ///< Entity states.
};

/**
 * @brief Encodes a snapshot.
 * @param writer Destination buffer.
 * @param payload Snapshot with at most SNAPSHOT_MAX_ENTITIES states.
 * @return True on success, false if there are too many states or no room.
 */
inline bool writeSnapshot(PacketWriter &writer,
                          const SnapshotPayload &payload) {
  if (payload.entities.size() > SNAPSHOT_MAX_ENTITIES)
    return false;

  const SnapshotHeader header{
      payload.serverTick, static_cast<std::uint16_t>(payload.entities.size())};

  if (!writePayload(writer, header))
    return false;

  for (const auto &state : payload.entities) {
    if (!writePayload(writer, state))
      return false;
  }
  return true;
}

/**
 * @brief Decodes and validates a snapshot.
 * @param reader Buffer holding exactly one snapshot payload.
 * @param payload Receives the snapshot, left untouched on failure.
 * @return True if the size, count and entity ids are all valid.
 */
inline bool readSnapshot(PacketReader &reader, SnapshotPayload &payload) {
  SnapshotHeader header;

  if (!readPayload(reader, header))
    return false;

  const std::size_t count = header.entityCount.get();

  if (count > SNAPSHOT_MAX_ENTITIES ||
      reader.getRemaining() != count * sizeof(SnapshotEntityState))
    return false;

  std::vector<SnapshotEntityState> states(count);
  std::unordered_set<std::uint32_t> ids;
  ids.reserve(count);

  for (auto &state : states) {
    if (!readPayload(reader, state))
      return false;

    if (state.networkId.get() == 0 || !ids.insert(state.networkId.get()).second)
      return false;
  }

  payload.serverTick = header.serverTick;
  payload.entities = std::move(states);
  return true;
}

/** @} */
} // namespace rtype::protocol
