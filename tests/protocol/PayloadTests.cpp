#include "Test.hpp"

#include <cstdint>
#include <numbers>
#include <set>
#include <vector>

#include "NetworkIdGenerator.hpp"
#include "Protocol/DisconnectPayload.hpp"
#include "Protocol/EntityPayload.hpp"
#include "Protocol/InputPayload.hpp"
#include "Protocol/MessageType.hpp"
#include "Protocol/PayloadCodec.hpp"
#include "Protocol/SnapshotPayload.hpp"
#include "protocol/PacketReader.hpp"
#include "protocol/PacketWriter.hpp"

using namespace rtype::protocol;

namespace {

template <typename T> std::vector<std::uint8_t> encode(const T &payload) {
  PacketWriter writer;
  writePayload(writer, payload);
  return writer.getData();
}

template <typename T>
bool decode(const std::vector<std::uint8_t> &bytes, T &payload) {
  PacketReader reader(bytes);
  return readPayload(reader, payload) && reader.isEnd();
}

SnapshotEntityState state(std::uint32_t id, float x = 0.0f, float y = 0.0f) {
  SnapshotEntityState entity{};
  entity.networkId = id;
  entity.entityType = static_cast<std::uint8_t>(id & 0xFF);
  entity.positionX = encodePosition(x);
  entity.positionY = encodePosition(y);
  entity.rotation = encodeRotation(0.0f);
  return entity;
}

std::vector<std::uint8_t> encodeSnapshot(const SnapshotPayload &payload) {
  PacketWriter writer;
  writeSnapshot(writer, payload);
  return writer.getData();
}

bool decodeSnapshot(const std::vector<std::uint8_t> &bytes,
                    SnapshotPayload &payload) {
  PacketReader reader(bytes);
  return readSnapshot(reader, payload);
}

} // namespace

// Input

TEST(InputPayload, IsNineBytes) {
  CHECK_EQ(sizeof(InputPayload), 9u);
  CHECK_EQ(encode(InputPayload{}).size(), 9u);
}

TEST(InputPayload, Layout) {
  InputPayload input{};
  input.inputSequence = 0x01020304;
  input.clientTick = 0x0A0B0C0D;
  input.actions = InputAction::Up | InputAction::Shoot;

  CHECK(encode(input) ==
        (std::vector<std::uint8_t>{1, 2, 3, 4, 0x0A, 0x0B, 0x0C, 0x0D, 0x11}));
}

TEST(InputPayload, RoundTrips) {
  InputPayload input{};
  input.inputSequence = 77;
  input.clientTick = 1234;
  input.actions = InputAction::Left | InputAction::Down;

  InputPayload decoded{};
  CHECK(decode(encode(input), decoded));
  CHECK_EQ(decoded.inputSequence.get(), 77u);
  CHECK_EQ(decoded.clientTick.get(), 1234u);
  CHECK_EQ(decoded.actions, input.actions);
}

TEST(InputPayload, ActionsAreDistinctBits) {
  const InputAction actions[] = {InputAction::Up,    InputAction::Down,
                                 InputAction::Left,  InputAction::Right,
                                 InputAction::Shoot, InputAction::SwitchWeapon};
  unsigned seen = 0;
  for (auto action : actions) {
    const unsigned bit = static_cast<std::uint8_t>(action);
    CHECK_EQ(bit & (bit - 1), 0u);
    CHECK_EQ(seen & bit, 0u);
    seen |= bit;
  }
  CHECK_EQ(static_cast<std::uint8_t>(InputAction::NoneAction), 0);
}

TEST(InputPayload, HasActionTestsOneBit) {
  const auto held = InputAction::Up | InputAction::Right;

  CHECK(hasAction(held, InputAction::Up));
  CHECK(hasAction(held, InputAction::Right));
  CHECK_FALSE(hasAction(held, InputAction::Down));
  CHECK_FALSE(hasAction(InputAction::NoneAction, InputAction::Shoot));
}

TEST(InputPayload, OrCombinesActions) {
  static_assert(
      static_cast<std::uint8_t>(InputAction::Up | InputAction::Down) == 0x03);
  CHECK_EQ(
      static_cast<std::uint8_t>(InputAction::Shoot | InputAction::SwitchWeapon),
      0x30);
}

TEST(InputPayload, ShortBufferIsRejected) {
  const std::vector<std::uint8_t> bytes(8, 0);
  PacketReader reader(bytes);
  InputPayload decoded{};

  CHECK_FALSE(readPayload(reader, decoded));
}

// Entities

TEST(EntityPayload, SizesHaveNoPadding) {
  CHECK_EQ(sizeof(CreateEntityPayload), 16u);
  CHECK_EQ(sizeof(DestroyEntityPayload), 9u);
  CHECK_EQ(sizeof(QueryEntityPayload), 4u);
}

TEST(EntityPayload, CreateEntityEncodesTheFields) {
  const auto payload = createEntityPayload(10, 1001, 42, 100.5f, -20.25f);

  CHECK_EQ(payload.serverTick.get(), 10u);
  CHECK_EQ(payload.networkId.get(), 1001u);
  CHECK_EQ(payload.entityType.get(), 42);
  CHECK_EQ(decodePosition(payload.positionX), 100.5f);
  CHECK_EQ(decodePosition(payload.positionY), -20.25f);
  CHECK_EQ(payload.rotation.get(), 0);
}

TEST(EntityPayload, CreateEntityEncodesTheRotation) {
  const auto payload =
      createEntityPayload(1, 2, 3, 0.0f, 0.0f, std::numbers::pi_v<float>);

  CHECK_EQ(payload.rotation.get(), 32768);
}

TEST(EntityPayload, CreateEntityLayout) {
  const auto bytes =
      encode(createEntityPayload(0x01020304, 0x0A0B0C0D, 0x1122, 0.0f, 0.0f));

  CHECK_EQ(bytes.size(), 16u);
  CHECK_EQ(bytes[0], 0x01);
  CHECK_EQ(bytes[4], 0x0A);
  CHECK_EQ(bytes[8], 0x11);
  CHECK_EQ(bytes[9], 0x22);
  CHECK_EQ(bytes[10], 0x80); // x = 0 encodes to 0x8000
  CHECK_EQ(bytes[11], 0x00);
}

TEST(EntityPayload, CreateEntityRoundTrips) {
  const auto payload = createEntityPayload(5, 6, 7, 1.0f, 2.0f, 0.5f);

  CreateEntityPayload decoded{};
  CHECK(decode(encode(payload), decoded));
  CHECK_EQ(decoded.networkId.get(), 6u);
  CHECK_EQ(decoded.entityType.get(), 7);
  CHECK_EQ(decodePosition(decoded.positionY), 2.0f);
  CHECK_EQ(decoded.rotation.get(), payload.rotation.get());
}

TEST(EntityPayload, CreateEntityNeedsAnId) {
  CHECK(isValid(createEntityPayload(1, 1, 0, 0, 0)));
  CHECK_FALSE(isValid(createEntityPayload(1, 0, 0, 0, 0)));
}

TEST(EntityPayload, DestroyEntityRoundTrips) {
  const auto payload =
      destroyEntityPayload(99, 1234, DestroyReason::OutOfBounds);

  DestroyEntityPayload decoded{};
  CHECK(decode(encode(payload), decoded));
  CHECK_EQ(decoded.serverTick.get(), 99u);
  CHECK_EQ(decoded.networkId.get(), 1234u);
  CHECK_EQ(decoded.reason, DestroyReason::OutOfBounds);
}

TEST(EntityPayload, DestroyEntityValidation) {
  CHECK(isValid(destroyEntityPayload(1, 1, DestroyReason::Unknown)));
  CHECK(isValid(destroyEntityPayload(1, 1, DestroyReason::GameEnded)));
  CHECK_FALSE(isValid(destroyEntityPayload(1, 0, DestroyReason::Collision)));
  CHECK_FALSE(
      isValid(destroyEntityPayload(1, 1, static_cast<DestroyReason>(0x05))));
}

TEST(EntityPayload, QueryEntityNeedsAnId) {
  QueryEntityPayload query{};
  CHECK_FALSE(isValid(query));
  query.networkId = 3;
  CHECK(isValid(query));

  QueryEntityPayload decoded{};
  CHECK(decode(encode(query), decoded));
  CHECK_EQ(decoded.networkId.get(), 3u);
}

TEST(EntityPayload, TruncatedCreateIsRejected) {
  auto bytes = encode(createEntityPayload(1, 2, 3, 4, 5));
  bytes.pop_back();
  CreateEntityPayload decoded{};

  CHECK_FALSE(decode(bytes, decoded));
}

// Disconnect

TEST(DisconnectPayload, IsOneByte) {
  CHECK_EQ(sizeof(DisconnectPayload), 1u);
  CHECK_EQ(DisconnectPayload{}.reason, DisconnectReason::ClientQuit);
}

TEST(DisconnectPayload, KnownReasonsAreValid) {
  for (auto reason :
       {DisconnectReason::ClientQuit, DisconnectReason::Timeout,
        DisconnectReason::ProtocolError, DisconnectReason::ServerShutdown})
    CHECK(isValid(DisconnectPayload{reason}));
}

TEST(DisconnectPayload, UnknownReasonsAreInvalid) {
  CHECK_FALSE(isValid(DisconnectPayload{static_cast<DisconnectReason>(0)}));
  CHECK_FALSE(isValid(DisconnectPayload{static_cast<DisconnectReason>(5)}));
  CHECK_FALSE(isValid(DisconnectPayload{static_cast<DisconnectReason>(0xFF)}));
}

// Snapshot

TEST(Snapshot, EmptySnapshotRoundTrips) {
  SnapshotPayload payload{};
  payload.serverTick = 600;

  const auto bytes = encodeSnapshot(payload);
  CHECK_EQ(bytes.size(), sizeof(SnapshotHeader));
  SnapshotPayload decoded{};
  CHECK(decodeSnapshot(bytes, decoded));
  CHECK_EQ(decoded.serverTick.get(), 600u);
  CHECK(decoded.entities.empty());
}

TEST(Snapshot, EntitiesRoundTrip) {
  SnapshotPayload payload{};
  payload.serverTick = 42;
  payload.entities = {state(1001, 10, 20), state(1002, -5, 300.5f)};

  SnapshotPayload decoded{};
  CHECK(decodeSnapshot(encodeSnapshot(payload), decoded));
  CHECK_EQ(decoded.entities.size(), 2u);
  CHECK_EQ(decoded.entities[0].networkId.get(), 1001u);
  CHECK_EQ(decoded.entities[1].networkId.get(), 1002u);
  CHECK_EQ(decodePosition(decoded.entities[1].positionY), 300.5f);
  CHECK_EQ(decoded.entities[1].entityType, 1002 & 0xFF);
}

TEST(Snapshot, SizeIsHeaderPlusStates) {
  SnapshotPayload payload{};
  payload.entities = {state(1), state(2), state(3)};

  CHECK_EQ(encodeSnapshot(payload).size(),
           sizeof(SnapshotHeader) + 3 * sizeof(SnapshotEntityState));
  CHECK_EQ(sizeof(SnapshotEntityState), 11u);
}

TEST(Snapshot, HeaderStartsWithTickAndCount) {
  SnapshotPayload payload{};
  payload.serverTick = 0x01020304;
  payload.entities = {state(1), state(2)};

  const auto bytes = encodeSnapshot(payload);
  CHECK_EQ(bytes[0], 0x01);
  CHECK_EQ(bytes[3], 0x04);
  CHECK_EQ(bytes[4], 0x00);
  CHECK_EQ(bytes[5], 0x02);
}

TEST(Snapshot, MaxEntitiesFitInAMessage) {
  CHECK(MessageHeader::HEADER_SIZE + sizeof(SnapshotHeader) +
            SNAPSHOT_MAX_ENTITIES * sizeof(SnapshotEntityState) <=
        MAX_MESSAGE_SIZE);
  CHECK(MessageHeader::HEADER_SIZE + sizeof(SnapshotHeader) +
            (SNAPSHOT_MAX_ENTITIES + 1) * sizeof(SnapshotEntityState) >
        MAX_MESSAGE_SIZE);
}

TEST(Snapshot, FullSnapshotRoundTrips) {
  SnapshotPayload payload{};
  for (std::uint32_t id = 1; id <= SNAPSHOT_MAX_ENTITIES; ++id)
    payload.entities.push_back(state(id));

  SnapshotPayload decoded{};
  CHECK(decodeSnapshot(encodeSnapshot(payload), decoded));
  CHECK_EQ(decoded.entities.size(), SNAPSHOT_MAX_ENTITIES);
}

TEST(Snapshot, TooManyEntitiesAreRefused) {
  SnapshotPayload payload{};
  for (std::uint32_t id = 1; id <= SNAPSHOT_MAX_ENTITIES + 1; ++id)
    payload.entities.push_back(state(id));
  PacketWriter writer;

  CHECK_FALSE(writeSnapshot(writer, payload));
  CHECK(writer.empty());
}

TEST(Snapshot, CountMismatchIsRejected) {
  SnapshotPayload payload{};
  payload.entities = {state(1), state(2)};
  auto bytes = encodeSnapshot(payload);
  bytes[5] = 3;

  SnapshotPayload decoded{};
  CHECK_FALSE(decodeSnapshot(bytes, decoded));
}

TEST(Snapshot, TrailingBytesAreRejected) {
  SnapshotPayload payload{};
  payload.entities = {state(1)};
  auto bytes = encodeSnapshot(payload);
  bytes.push_back(0);

  SnapshotPayload decoded{};
  CHECK_FALSE(decodeSnapshot(bytes, decoded));
}

TEST(Snapshot, ZeroIdIsRejected) {
  SnapshotPayload payload{};
  payload.entities = {state(1), state(0)};

  SnapshotPayload decoded{};
  CHECK_FALSE(decodeSnapshot(encodeSnapshot(payload), decoded));
}

TEST(Snapshot, DuplicateIdIsRejected) {
  SnapshotPayload payload{};
  payload.entities = {state(5), state(6), state(5)};

  SnapshotPayload decoded{};
  CHECK_FALSE(decodeSnapshot(encodeSnapshot(payload), decoded));
}

TEST(Snapshot, TruncatedHeaderIsRejected) {
  const std::vector<std::uint8_t> bytes = {0, 0, 0};
  SnapshotPayload decoded{};

  CHECK_FALSE(decodeSnapshot(bytes, decoded));
}

TEST(Snapshot, FailureKeepsThePreviousPayload) {
  SnapshotPayload decoded{};
  decoded.serverTick = 7;
  decoded.entities = {state(9)};
  SnapshotPayload bad{};
  bad.serverTick = 8;
  bad.entities = {state(1), state(1)};

  CHECK_FALSE(decodeSnapshot(encodeSnapshot(bad), decoded));
  CHECK_EQ(decoded.serverTick.get(), 7u);
  CHECK_EQ(decoded.entities.size(), 1u);
}

// Message types

TEST(MessageType, IdsMatchTheProtocol) {
  CHECK_EQ(toMessageId(MessageType::Input), 0x02);
  CHECK_EQ(toMessageId(MessageType::QueryEntity), 0x13);
  CHECK_EQ(toMessageId(MessageType::ListRooms), 0x20);
  CHECK_EQ(toMessageId(MessageType::SetReady), 0x24);
  CHECK_EQ(toMessageId(MessageType::Snapshot), 0x83);
  CHECK_EQ(toMessageId(MessageType::CreateEntity), 0x84);
  CHECK_EQ(toMessageId(MessageType::DestroyEntity), 0x85);
  CHECK_EQ(toMessageId(MessageType::RoomList), 0x90);
  CHECK_EQ(toMessageId(MessageType::GameStarting), 0x95);
}

TEST(MessageType, SystemMessagesShareTheirIds) {
  CHECK_EQ(toMessageId(MessageType::Ping), systemMessage::Ping);
  CHECK_EQ(toMessageId(MessageType::Pong), systemMessage::Pong);
  CHECK_EQ(toMessageId(MessageType::Disconnect), systemMessage::Disconnect);
}

TEST(MessageType, InvalidIsZero) {
  CHECK_EQ(toMessageId(MessageType::Invalid), 0);
}

TEST(MessageType, EntityLifecycleIsCritical) {
  CHECK_EQ(messagePriority(MessageType::CreateEntity),
           PacketPriority::Critical);
  CHECK_EQ(messagePriority(MessageType::DestroyEntity),
           PacketPriority::Critical);
}

TEST(MessageType, RoomMessagesAreReliable) {
  for (auto type :
       {MessageType::QueryEntity, MessageType::ListRooms,
        MessageType::CreateRoom, MessageType::JoinRoom, MessageType::LeaveRoom,
        MessageType::SetReady, MessageType::RoomList, MessageType::RoomCreated,
        MessageType::RoomJoined, MessageType::RoomLeft,
        MessageType::RoomStateUpdate, MessageType::GameStarting})
    CHECK_EQ(messagePriority(type), PacketPriority::Reliable);
}

TEST(MessageType, StreamedMessagesAreUnreliable) {
  for (auto type : {MessageType::Input, MessageType::Snapshot,
                    MessageType::Ping, MessageType::Pong})
    CHECK_EQ(messagePriority(type), PacketPriority::Unreliable);
}

TEST(MessageType, ServerToClientIdsHaveTheHighBit) {
  for (auto type : {MessageType::Snapshot, MessageType::CreateEntity,
                    MessageType::DestroyEntity, MessageType::Pong,
                    MessageType::RoomList, MessageType::GameStarting})
    CHECK((toMessageId(type) & 0x80) != 0);
  for (auto type : {MessageType::Input, MessageType::QueryEntity,
                    MessageType::CreateRoom, MessageType::SetReady})
    CHECK((toMessageId(type) & 0x80) == 0);
}

// NetworkIdGenerator

TEST(NetworkIdGenerator, IsASingleton) {
  auto &first = rtype::server::NetworkIdGenerator::getInstance();
  auto &second = rtype::server::NetworkIdGenerator::getInstance();

  CHECK(&first == &second);
}

TEST(NetworkIdGenerator, IdsIncreaseByOne) {
  auto &generator = rtype::server::NetworkIdGenerator::getInstance();

  const auto first = generator.getNextId();
  CHECK_EQ(generator.getNextId(), first + 1);
  CHECK_EQ(generator.getNextId(), first + 2);
}

TEST(NetworkIdGenerator, IdsAreNeverZeroAndUnique) {
  auto &generator = rtype::server::NetworkIdGenerator::getInstance();
  std::set<std::uint32_t> ids;

  for (int i = 0; i < 1000; ++i) {
    const auto id = generator.getNextId();
    CHECK(id >= 1000u);
    CHECK(ids.insert(id).second);
  }
}
