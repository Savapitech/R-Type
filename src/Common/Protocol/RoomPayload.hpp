#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <utility>

#include "BigEndian.hpp"
#include "PayloadCodec.hpp"

namespace rtype::protocol {

/**
 * @brief Maximum room name length transmitted over the network.
 */
inline constexpr std::size_t MAX_ROOM_NAME_LENGTH = 32;

/**
 * @brief Maximum number of rooms in one RoomList message.
 *
 * A larger room directory will require pagination.
 */
inline constexpr std::size_t MAX_ROOMS_PER_LIST = 8;

/**
 * @brief Maximum number of players represented in a room.
 */
inline constexpr std::size_t MAX_ROOM_PLAYERS = 4;

/**
 * @brief Result of a room management request.
 */
enum class RoomResult : std::uint8_t {
    Success = 0,
    RoomNotFound = 1,
    RoomFull = 2,
    RoomAlreadyPlaying = 3,
    AlreadyInRoom = 4,
    NotInRoom = 5,
    InvalidRequest = 6,
    InternalError = 7
};

/**
 * @brief Lifecycle state transmitted to clients.
 */
enum class RoomStatus : std::uint8_t {
    Waiting = 0,
    Playing = 1,
    Finished = 2
};

/**
 * @brief CreateRoom request payload.
 */
struct CreateRoomPayload {
    std::string name; ///< Requested room name.
};

/**
 * @brief JoinRoom request payload.
 */
struct JoinRoomPayload {
    be32 roomId; ///< Room to join.
};

/**
 * @brief SetReady request payload.
 */
struct SetReadyPayload {
    std::uint8_t ready = 0; ///< 0 = not ready, 1 = ready.
};

/**
 * @brief Generic response to a room management request.
 *
 * Used for RoomCreated, RoomJoined and RoomLeft.
 */
struct RoomResultPayload {
    RoomResult result = RoomResult::Success; ///< Operation result.
    be32 roomId; ///< Affected room, or 0 if not applicable.
};

/**
 * @brief Information about a room sent to clients.
 */
struct RoomInfoPayload {
    std::uint32_t roomId = 0; ///< Room identifier.
    std::string name; ///< Display name.
    RoomStatus status = RoomStatus::Waiting; ///< Room state.
    std::uint8_t playerCount = 0; ///< Current player count.
    std::uint8_t maxPlayers = 4; ///< Room capacity.
};

/**
 * @brief RoomList response payload.
 */
struct RoomListPayload {
    std::vector<RoomInfoPayload> rooms; ///< Available rooms.
};

/**
 * @brief Ready state of one connected player.
 */
struct RoomPlayerState {
    std::uint32_t connectionId = 0; ///< Player connection ID.
    bool ready = false; ///< Whether the player is ready.
};

/**
 * @brief RoomStateUpdate response payload.
 */
struct RoomStateUpdatePayload {
    std::uint32_t roomId = 0; ///< Updated room.
    std::vector<RoomPlayerState> players; ///< Player ready states.
};

/**
 * @brief GameStarting notification payload.
 */
struct GameStartingPayload {
    be32 roomId; ///< Room whose game is starting.
};

static_assert(sizeof(JoinRoomPayload) == 4);
static_assert(sizeof(SetReadyPayload) == 1);

/**
 * @brief Writes a length-prefixed room name.
 * @param writer Destination writer.
 * @param name Room name.
 * @return True on success.
 */
inline bool writeRoomName(PacketWriter &writer, const std::string &name) {
    if (name.size() > MAX_ROOM_NAME_LENGTH)
        return false;
    if (!writer.writeUInt8(static_cast<std::uint8_t>(name.size())))
        return false;
    return writer.writeBytes(reinterpret_cast<const std::uint8_t *>(name.data()), name.size());
}

/**
 * @brief Reads a length-prefixed room name.
 * @param reader Source reader.
 * @param name Receives the decoded name.
 * @return True on success.
 */
inline bool readRoomName(PacketReader &reader, std::string &name) {
    std::uint8_t length = 0;
    if (!reader.readUInt8(length))
        return false;
    if (length > MAX_ROOM_NAME_LENGTH)
        return false;
    std::string decoded(length, '\0');
    if (!reader.readBytes(reinterpret_cast<std::uint8_t *>(decoded.data()), length))
        return false;
    name = std::move(decoded);
    return true;
}

/**
 * @brief Encodes a CreateRoom request.
 * @param writer Destination writer.
 * @param payload Request to encode.
 * @return True on success.
 */
inline bool writeCreateRoom(PacketWriter &writer, const CreateRoomPayload &payload) {
    return writeRoomName(writer, payload.name);
}

/**
 * @brief Decodes a CreateRoom request.
 * @param reader Source reader.
 * @param payload Receives the decoded request.
 * @return True if the payload is valid.
 */
inline bool readCreateRoom(PacketReader &reader, CreateRoomPayload &payload) {
    std::string name;
    if (!readRoomName(reader, name) || !reader.isEnd())
        return false;
    if (name.empty())
        return false;
    payload.name = std::move(name);
    return true;
}

/**
 * @brief Validates and decodes a JoinRoom request.
 * @param reader Source reader.
 * @param payload Receives the request.
 * @return True if the room ID is valid.
 */
inline bool readJoinRoom(PacketReader &reader, JoinRoomPayload &payload) {
    JoinRoomPayload decoded{};
    if (!readPayload(reader, decoded) || !reader.isEnd() || decoded.roomId.get() == 0)
        return false;
    payload = decoded;
    return true;
}

/**
 * @brief Validates and decodes a SetReady request.
 * @param reader Source reader.
 * @param payload Receives the request.
 * @return True if the ready value is valid.
 */
inline bool readSetReady(PacketReader &reader, SetReadyPayload &payload)
{
    SetReadyPayload decoded{};
    if (!readPayload(reader, decoded) || !reader.isEnd() || decoded.ready > 1)
        return false;
    payload = decoded;
    return true;
}

/**
 * @brief Encodes a room management result.
 * @param writer Destination writer.
 * @param payload Result to encode.
 * @return True on success.
 */
inline bool writeRoomResult(PacketWriter &writer, const RoomResultPayload &payload)
{
    return writer.writeUInt8(static_cast<std::uint8_t>(payload.result)) &&
           writer.writeUInt32(payload.roomId.get());
}

/**
 * @brief Decodes a room management result.
 * @param reader Source reader.
 * @param payload Receives the result.
 * @return True if the result is valid.
 */
inline bool readRoomResult(PacketReader &reader, RoomResultPayload &payload) {
    std::uint8_t result = 0;
    std::uint32_t roomId = 0;

    if (!reader.readUInt8(result) || !reader.readUInt32(roomId) ||
        !reader.isEnd() || result > static_cast<std::uint8_t>(RoomResult::InternalError))
        return false;
    payload.result = static_cast<RoomResult>(result);
    payload.roomId = roomId;
    return true;
}

/**
 * @brief Encodes a list of available rooms.
 * @param writer Destination writer.
 * @param payload List to encode.
 * @return True on success.
 */
inline bool writeRoomList(PacketWriter &writer, const RoomListPayload &payload) {
    if (payload.rooms.size() > MAX_ROOMS_PER_LIST)
        return false;
    if (!writer.writeUInt8(static_cast<std::uint8_t>(payload.rooms.size())))
        return false;

    for (const auto &room : payload.rooms) {
        if (room.roomId == 0 ||
            room.name.size() > MAX_ROOM_NAME_LENGTH ||
            room.playerCount > room.maxPlayers ||
            room.maxPlayers > MAX_ROOM_PLAYERS ||
            room.maxPlayers == 0 ||
            static_cast<std::uint8_t>(room.status) > 2)
            return false;

        if (!writer.writeUInt32(room.roomId) ||
          !writer.writeUInt8(
              static_cast<std::uint8_t>(room.status)) ||
          !writer.writeUInt8(room.playerCount) ||
          !writer.writeUInt8(room.maxPlayers) ||
          !writeRoomName(writer, room.name))
          return false;
    }
    return true;
}

/**
 * @brief Decodes a list of available rooms.
 * @param reader Source reader.
 * @param payload Receives the list.
 * @return True if the list is valid.
 */
inline bool readRoomList(PacketReader &reader, RoomListPayload &payload) {
    std::uint8_t count = 0;
    if (!reader.readUInt8(count) ||
        count > MAX_ROOMS_PER_LIST)
        return false;
    RoomListPayload decoded;
    for (std::size_t i = 0; i < count; ++i) {
        RoomInfoPayload room;
        std::uint8_t status = 0;
        if (!reader.readUInt32(room.roomId) ||
            !reader.readUInt8(status) ||
            !reader.readUInt8(room.playerCount) ||
            !reader.readUInt8(room.maxPlayers) ||
            !readRoomName(reader, room.name))
            return false;

        if (room.roomId == 0 ||
            status > 2 ||
            room.maxPlayers == 0 ||
            room.maxPlayers > MAX_ROOM_PLAYERS ||
            room.playerCount > room.maxPlayers)
            return false;

        room.status = static_cast<RoomStatus>(status);
        decoded.rooms.push_back(std::move(room));
    }

    if (!reader.isEnd())
        return false;

    payload = std::move(decoded);
    return true;
}

/**
 * @brief Encodes the ready states of a room's players.
 * @param writer Destination writer.
 * @param payload State update to encode.
 * @return True on success.
 */
inline bool writeRoomStateUpdate(
    PacketWriter &writer,
    const RoomStateUpdatePayload &payload)
{
    if (payload.roomId == 0 ||
        payload.players.size() > MAX_ROOM_PLAYERS)
        return false;

    if (!writer.writeUInt32(payload.roomId) ||
        !writer.writeUInt8(
            static_cast<std::uint8_t>(payload.players.size())))
        return false;

    for (const auto &player : payload.players) {
        if (!writer.writeUInt32(player.connectionId) ||
            !writer.writeUInt8(player.ready ? 1 : 0))
            return false;
    }

    return true;
}

/**
 * @brief Decodes a room ready-state update.
 * @param reader Source reader.
 * @param payload Receives the update.
 * @return True if the payload is valid.
 */
inline bool readRoomStateUpdate(
    PacketReader &reader,
    RoomStateUpdatePayload &payload)
{
    RoomStateUpdatePayload decoded;
    std::uint8_t count = 0;

    if (!reader.readUInt32(decoded.roomId) ||
        !reader.readUInt8(count) ||
        decoded.roomId == 0 ||
        count > MAX_ROOM_PLAYERS)
        return false;

    for (std::size_t i = 0; i < count; ++i) {
        RoomPlayerState player;
        std::uint8_t ready = 0;

        if (!reader.readUInt32(player.connectionId) ||
            !reader.readUInt8(ready) ||
            ready > 1)
            return false;

        player.ready = (ready == 1);
        decoded.players.push_back(player);
    }

    if (!reader.isEnd())
        return false;

    payload = std::move(decoded);
    return true;
}

} // namespace rtype::protocol
