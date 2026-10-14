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

struct SnapshotHeader {
  be32 serverTick;
  be16 entityCount;
};

struct SnapshotEntityState {
  be32 networkId;
  std::uint8_t entityType = 0;
  be16 positionX;
  be16 positionY;
  be16 rotation;
};

static_assert(sizeof(SnapshotHeader) == sizeof(be32) + sizeof(be16),
              "SnapshotHeader must not contain padding");
static_assert(sizeof(SnapshotEntityState) ==
                  sizeof(be32) + sizeof(std::uint8_t) + sizeof(be16) * 3,
              "SnapshotEntityState must not contain padding");

inline constexpr std::size_t SNAPSHOT_MAX_ENTITIES =
    (MAX_MESSAGE_SIZE - MessageHeader::HEADER_SIZE - sizeof(SnapshotHeader)) /
    sizeof(SnapshotEntityState);

struct SnapshotPayload {
  be32 serverTick;
  std::vector<SnapshotEntityState> entities;
};

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

} // namespace rtype::protocol
