#pragma once

#include "EntityPayload.hpp"

#include "protocol/MessageHeader.hpp"

#include <cstddef>
#include <cstdint>
#include <unordered_set>
#include <utility>
#include <vector>

namespace rtype::protocol {

struct SnapshotEntityState {
    static constexpr std::size_t SIZE = 8;

    NetworkEntityId entityId = 0;
    float x = 0.0f;
    float y = 0.0f;
};

struct SnapshotPayload {
    static constexpr std::size_t HEADER_SIZE = 5;
    static constexpr std::size_t MAX_SIZE = MAX_MESSAGE_SIZE - MessageHeader::HEADER_SIZE;
    static constexpr std::size_t MAX_ENTITIES = (MAX_SIZE - HEADER_SIZE) / SnapshotEntityState::SIZE;
    std::uint32_t serverTick = 0;
    std::vector<SnapshotEntityState> entities;

    std::size_t serializedSize() const {
        return HEADER_SIZE + entities.size() * SnapshotEntityState::SIZE;
    }

    bool serialize(PacketWriter &writer) const {
        if (entities.size() > MAX_ENTITIES)
            return false;

        const std::size_t size = serializedSize();

        if (writer.getSize() > MAX_PAYLOAD_SIZE || size > MAX_PAYLOAD_SIZE - writer.getSize())
            return false;

        std::unordered_set<NetworkEntityId> ids;
        ids.reserve(entities.size());

        for (const auto &entity : entities) {
            std::uint16_t encodedX = 0;
            std::uint16_t encodedY = 0;

            if (entity.entityId == 0 || !ids.insert(entity.entityId).second ||
                !encodePosition(entity.x, encodedX) || !encodePosition(entity.y, encodedY))
                return false;
        }

        if (!writer.writeUInt32(serverTick) ||
            !writer.writeUInt8(static_cast<std::uint8_t>(entities.size()))) 
            return false;

        for (const auto &entity : entities) {
            std::uint16_t encodedX = 0;
            std::uint16_t encodedY = 0;

            if (!encodePosition(entity.x, encodedX) || !encodePosition(entity.y, encodedY))
                return false;

            if (!writer.writeUInt32(entity.entityId) || !writer.writeUInt16(encodedX) || !writer.writeUInt16(encodedY))
                return false;
        }
        return true;
    }

    bool deserialize(PacketReader &reader)
    {
        std::uint32_t tick = 0;
        std::uint8_t count = 0;

        if (!reader.readUInt32(tick) || !reader.readUInt8(count))
            return false;

        if (count > MAX_ENTITIES)
            return false;

        const std::size_t expectedSize = static_cast<std::size_t>(count) * SnapshotEntityState::SIZE;

        if (reader.getRemaining() != expectedSize)
            return false;

        std::vector<SnapshotEntityState> decoded;
        decoded.reserve(count);
        std::unordered_set<NetworkEntityId> ids;
        ids.reserve(count);

        for (std::size_t i = 0; i < count; ++i) {
            std::uint32_t id = 0;
            std::uint16_t encodedX = 0;
            std::uint16_t encodedY = 0;

            if (!reader.readUInt32(id) || !reader.readUInt16(encodedX) || !reader.readUInt16(encodedY))
                return false;

            if (id == 0 || !ids.insert(id).second)
                return false;

            decoded.push_back({id, decodePosition(encodedX), decodePosition(encodedY)});
        }

        serverTick = tick;
        entities = std::move(decoded);
        return true;
    }
};
} // namespace rtype::protocol
