#include "Test.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include "Protocol/RoomPayload.hpp"

using namespace rtype::protocol;

namespace {

using Bytes = std::vector<std::uint8_t>;

template <typename Write> Bytes written(Write write) {
  PacketWriter writer;
  write(writer);
  return writer.getData();
}

RoomInfoPayload room(std::uint32_t id, const std::string &name,
                     std::uint8_t players = 1, std::uint8_t maxPlayers = 4,
                     RoomStatus status = RoomStatus::Waiting) {
  RoomInfoPayload info;
  info.roomId = id;
  info.name = name;
  info.playerCount = players;
  info.maxPlayers = maxPlayers;
  info.status = status;
  return info;
}

} // namespace

// Room names

TEST(RoomName, IsLengthPrefixed) {
  const auto bytes = written([](PacketWriter &w) { writeRoomName(w, "abc"); });

  CHECK(bytes == (Bytes{3, 'a', 'b', 'c'}));
}

TEST(RoomName, RoundTrips) {
  const auto bytes =
      written([](PacketWriter &w) { writeRoomName(w, "Bydo hunters"); });
  PacketReader reader(bytes);
  std::string name;

  CHECK(readRoomName(reader, name));
  CHECK_EQ(name, std::string("Bydo hunters"));
  CHECK(reader.isEnd());
}

TEST(RoomName, AcceptsTheMaximumLength) {
  const std::string name(MAX_ROOM_NAME_LENGTH, 'x');
  PacketWriter writer;

  CHECK(writeRoomName(writer, name));
  PacketReader reader(writer.getData());
  std::string decoded;
  CHECK(readRoomName(reader, decoded));
  CHECK_EQ(decoded.size(), MAX_ROOM_NAME_LENGTH);
}

TEST(RoomName, RefusesLongerNames) {
  PacketWriter writer;

  CHECK_FALSE(
      writeRoomName(writer, std::string(MAX_ROOM_NAME_LENGTH + 1, 'x')));
  CHECK(writer.empty());
}

TEST(RoomName, RejectsALengthAboveTheMaximum) {
  Bytes bytes(1 + MAX_ROOM_NAME_LENGTH + 1, 'x');
  bytes[0] = static_cast<std::uint8_t>(MAX_ROOM_NAME_LENGTH + 1);
  PacketReader reader(bytes);
  std::string name;

  CHECK_FALSE(readRoomName(reader, name));
}

TEST(RoomName, RejectsATruncatedName) {
  const Bytes bytes = {5, 'a', 'b'};
  PacketReader reader(bytes);
  std::string name = "kept";

  CHECK_FALSE(readRoomName(reader, name));
  CHECK_EQ(name, std::string("kept"));
}

TEST(RoomName, EmptyNameIsEncodable) {
  const auto bytes = written([](PacketWriter &w) { writeRoomName(w, ""); });

  CHECK(bytes == Bytes{0});
}

// CreateRoom

TEST(CreateRoom, RoundTrips) {
  const auto bytes = written(
      [](PacketWriter &w) { writeCreateRoom(w, CreateRoomPayload{"lobby"}); });
  PacketReader reader(bytes);
  CreateRoomPayload payload;

  CHECK(readCreateRoom(reader, payload));
  CHECK_EQ(payload.name, std::string("lobby"));
}

TEST(CreateRoom, EmptyNameIsRejected) {
  const Bytes bytes = {0};
  PacketReader reader(bytes);
  CreateRoomPayload payload;

  CHECK_FALSE(readCreateRoom(reader, payload));
}

TEST(CreateRoom, TrailingBytesAreRejected) {
  const Bytes bytes = {2, 'o', 'k', '!'};
  PacketReader reader(bytes);
  CreateRoomPayload payload;

  CHECK_FALSE(readCreateRoom(reader, payload));
}

// JoinRoom and SetReady

TEST(JoinRoom, RoundTrips) {
  JoinRoomPayload join{};
  join.roomId = 0x00000102;
  PacketWriter writer;
  writePayload(writer, join);

  CHECK(writer.getData() == (Bytes{0, 0, 1, 2}));
  PacketReader reader(writer.getData());
  JoinRoomPayload decoded{};
  CHECK(readJoinRoom(reader, decoded));
  CHECK_EQ(decoded.roomId.get(), 0x102u);
}

TEST(JoinRoom, RoomZeroIsRejected) {
  const Bytes bytes = {0, 0, 0, 0};
  PacketReader reader(bytes);
  JoinRoomPayload decoded{};

  CHECK_FALSE(readJoinRoom(reader, decoded));
}

TEST(JoinRoom, WrongSizeIsRejected) {
  const Bytes shortBytes = {0, 0, 1};
  const Bytes longBytes = {0, 0, 0, 1, 0};
  PacketReader shortReader(shortBytes);
  PacketReader longReader(longBytes);
  JoinRoomPayload decoded{};

  CHECK_FALSE(readJoinRoom(shortReader, decoded));
  CHECK_FALSE(readJoinRoom(longReader, decoded));
}

TEST(SetReady, AcceptsZeroAndOne) {
  for (std::uint8_t value : {0, 1}) {
    const Bytes bytes = {value};
    PacketReader reader(bytes);
    SetReadyPayload decoded{};
    CHECK(readSetReady(reader, decoded));
    CHECK_EQ(decoded.ready, value);
  }
}

TEST(SetReady, RejectsOtherValues) {
  const Bytes bytes = {2};
  PacketReader reader(bytes);
  SetReadyPayload decoded{};

  CHECK_FALSE(readSetReady(reader, decoded));
}

TEST(SetReady, RejectsAnEmptyPayload) {
  const Bytes bytes;
  PacketReader reader(bytes);
  SetReadyPayload decoded{};

  CHECK_FALSE(readSetReady(reader, decoded));
}

// RoomResult

TEST(RoomResult, IsFiveBytes) {
  RoomResultPayload result;
  result.result = RoomResult::RoomFull;
  result.roomId = 9;
  const auto bytes =
      written([&](PacketWriter &w) { writeRoomResult(w, result); });

  CHECK(bytes == (Bytes{2, 0, 0, 0, 9}));
}

TEST(RoomResult, RoundTripsEveryResult) {
  for (std::uint8_t code = 0;
       code <= static_cast<std::uint8_t>(RoomResult::InternalError); ++code) {
    RoomResultPayload result;
    result.result = static_cast<RoomResult>(code);
    result.roomId = 1000 + code;
    const auto bytes =
        written([&](PacketWriter &w) { writeRoomResult(w, result); });
    PacketReader reader(bytes);
    RoomResultPayload decoded;
    CHECK(readRoomResult(reader, decoded));
    CHECK_EQ(decoded.result, result.result);
    CHECK_EQ(decoded.roomId.get(), 1000u + code);
  }
}

TEST(RoomResult, UnknownResultIsRejected) {
  const Bytes bytes = {8, 0, 0, 0, 1};
  PacketReader reader(bytes);
  RoomResultPayload decoded;

  CHECK_FALSE(readRoomResult(reader, decoded));
}

TEST(RoomResult, TrailingBytesAreRejected) {
  const Bytes bytes = {0, 0, 0, 0, 1, 0};
  PacketReader reader(bytes);
  RoomResultPayload decoded;

  CHECK_FALSE(readRoomResult(reader, decoded));
}

// RoomList

TEST(RoomList, EmptyListIsOneByte) {
  const auto bytes =
      written([](PacketWriter &w) { writeRoomList(w, RoomListPayload{}); });

  CHECK(bytes == Bytes{0});
  PacketReader reader(bytes);
  RoomListPayload decoded;
  CHECK(readRoomList(reader, decoded));
  CHECK(decoded.rooms.empty());
}

TEST(RoomList, RoundTrips) {
  RoomListPayload list;
  list.rooms = {room(1, "alpha", 2, 4),
                room(2, "beta", 4, 4, RoomStatus::Playing),
                room(3, "", 0, 1, RoomStatus::Finished)};
  const auto bytes = written([&](PacketWriter &w) { writeRoomList(w, list); });
  PacketReader reader(bytes);
  RoomListPayload decoded;

  CHECK(readRoomList(reader, decoded));
  CHECK_EQ(decoded.rooms.size(), 3u);
  CHECK_EQ(decoded.rooms[0].name, std::string("alpha"));
  CHECK_EQ(decoded.rooms[0].playerCount, 2);
  CHECK_EQ(decoded.rooms[1].status, RoomStatus::Playing);
  CHECK_EQ(decoded.rooms[2].roomId, 3u);
  CHECK_EQ(decoded.rooms[2].maxPlayers, 1);
}

TEST(RoomList, RefusesTooManyRooms) {
  RoomListPayload list;
  for (std::uint32_t id = 1; id <= MAX_ROOMS_PER_LIST + 1; ++id)
    list.rooms.push_back(room(id, "r"));
  PacketWriter writer;

  CHECK_FALSE(writeRoomList(writer, list));
}

TEST(RoomList, AcceptsTheMaximumNumberOfRooms) {
  RoomListPayload list;
  for (std::uint32_t id = 1; id <= MAX_ROOMS_PER_LIST; ++id)
    list.rooms.push_back(room(id, std::string(MAX_ROOM_NAME_LENGTH, 'n')));
  const auto bytes = written([&](PacketWriter &w) { writeRoomList(w, list); });
  PacketReader reader(bytes);
  RoomListPayload decoded;

  CHECK(readRoomList(reader, decoded));
  CHECK_EQ(decoded.rooms.size(), MAX_ROOMS_PER_LIST);
}

TEST(RoomList, RefusesInvalidRooms) {
  const RoomInfoPayload invalid[] = {
      room(0, "zero id"),
      room(1, "full", 5, 4),
      room(1, "too big", 1, MAX_ROOM_PLAYERS + 1),
      room(1, "no seat", 0, 0),
      room(1, std::string(MAX_ROOM_NAME_LENGTH + 1, 'x')),
      room(1, "status", 1, 4, static_cast<RoomStatus>(3)),
  };
  for (const auto &info : invalid) {
    RoomListPayload list;
    list.rooms = {info};
    PacketWriter writer;
    CHECK_FALSE(writeRoomList(writer, list));
  }
}

TEST(RoomList, RejectsInvalidRoomsOnRead) {
  // room 1, status 0, 3 players for 2 seats, name "a"
  const Bytes bytes = {1, 0, 0, 0, 1, 0, 3, 2, 1, 'a'};
  PacketReader reader(bytes);
  RoomListPayload decoded;

  CHECK_FALSE(readRoomList(reader, decoded));
}

TEST(RoomList, RejectsAWrongCount) {
  const Bytes bytes = {2, 0, 0, 0, 1, 0, 1, 4, 1, 'a'};
  PacketReader reader(bytes);
  RoomListPayload decoded;

  CHECK_FALSE(readRoomList(reader, decoded));
}

TEST(RoomList, RejectsTrailingBytes) {
  const Bytes bytes = {1, 0, 0, 0, 1, 0, 1, 4, 1, 'a', 0xFF};
  PacketReader reader(bytes);
  RoomListPayload decoded;

  CHECK_FALSE(readRoomList(reader, decoded));
}

TEST(RoomList, FailureKeepsThePreviousList) {
  RoomListPayload decoded;
  decoded.rooms = {room(7, "old")};
  const Bytes bytes = {9};
  PacketReader reader(bytes);

  CHECK_FALSE(readRoomList(reader, decoded));
  CHECK_EQ(decoded.rooms.size(), 1u);
  CHECK_EQ(decoded.rooms[0].roomId, 7u);
}

// RoomStateUpdate

TEST(RoomStateUpdate, RoundTrips) {
  RoomStateUpdatePayload update;
  update.roomId = 4;
  update.players = {{11, true}, {12, false}, {13, true}};
  const auto bytes =
      written([&](PacketWriter &w) { writeRoomStateUpdate(w, update); });
  PacketReader reader(bytes);
  RoomStateUpdatePayload decoded;

  CHECK_EQ(bytes.size(), 5u + 3u * 5u);
  CHECK(readRoomStateUpdate(reader, decoded));
  CHECK_EQ(decoded.roomId, 4u);
  CHECK_EQ(decoded.players.size(), 3u);
  CHECK_EQ(decoded.players[0].connectionId, 11u);
  CHECK(decoded.players[0].ready);
  CHECK_FALSE(decoded.players[1].ready);
}

TEST(RoomStateUpdate, EmptyRoomRoundTrips) {
  RoomStateUpdatePayload update;
  update.roomId = 1;
  const auto bytes =
      written([&](PacketWriter &w) { writeRoomStateUpdate(w, update); });
  PacketReader reader(bytes);
  RoomStateUpdatePayload decoded;

  CHECK(readRoomStateUpdate(reader, decoded));
  CHECK(decoded.players.empty());
}

TEST(RoomStateUpdate, RefusesRoomZero) {
  RoomStateUpdatePayload update;
  PacketWriter writer;

  CHECK_FALSE(writeRoomStateUpdate(writer, update));
}

TEST(RoomStateUpdate, RefusesTooManyPlayers) {
  RoomStateUpdatePayload update;
  update.roomId = 1;
  for (std::uint32_t id = 1; id <= MAX_ROOM_PLAYERS + 1; ++id)
    update.players.push_back({id, false});
  PacketWriter writer;

  CHECK_FALSE(writeRoomStateUpdate(writer, update));
}

TEST(RoomStateUpdate, RejectsAnInvalidReadyFlag) {
  const Bytes bytes = {0, 0, 0, 1, 1, 0, 0, 0, 5, 2};
  PacketReader reader(bytes);
  RoomStateUpdatePayload decoded;

  CHECK_FALSE(readRoomStateUpdate(reader, decoded));
}

TEST(RoomStateUpdate, RejectsTrailingBytes) {
  const Bytes bytes = {0, 0, 0, 1, 0, 0};
  PacketReader reader(bytes);
  RoomStateUpdatePayload decoded;

  CHECK_FALSE(readRoomStateUpdate(reader, decoded));
}

TEST(RoomStateUpdate, RejectsTooManyPlayersOnRead) {
  const Bytes bytes = {0, 0, 0, 1,
                       static_cast<std::uint8_t>(MAX_ROOM_PLAYERS + 1)};
  PacketReader reader(bytes);
  RoomStateUpdatePayload decoded;

  CHECK_FALSE(readRoomStateUpdate(reader, decoded));
}

// Game starting

TEST(GameStarting, IsTheRoomId) {
  GameStartingPayload payload{};
  payload.roomId = 0x0A0B0C0D;
  PacketWriter writer;

  CHECK(writePayload(writer, payload));
  CHECK(writer.getData() == (Bytes{0x0A, 0x0B, 0x0C, 0x0D}));
}

TEST(RoomLimits, MatchTheGame) {
  CHECK_EQ(MAX_ROOM_PLAYERS, 4u);
  CHECK(MAX_ROOM_NAME_LENGTH > 0);
  CHECK(MAX_ROOMS_PER_LIST > 0);
  CHECK_EQ(RoomInfoPayload{}.maxPlayers, MAX_ROOM_PLAYERS);
}
