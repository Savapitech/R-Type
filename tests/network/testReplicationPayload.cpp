#include "Protocol/EntityPayload.hpp"
#include "Protocol/MessageType.hpp"
#include "Protocol/SnapshotPayload.hpp"

#include "protocol/MessageHeader.hpp"
#include "protocol/PacketReader.hpp"
#include "protocol/PacketWriter.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

#define CHECK(condition)                                      \
    do {                                                      \
        if (!(condition)) {                                   \
            std::cerr << "CHECK FAILED: " #condition          \
                      << " at " << __FILE__ << ":"             \
                      << __LINE__ << '\n';                    \
            std::exit(EXIT_FAILURE);                          \
        }                                                     \
    } while (false)

namespace {

using namespace rtype::protocol;

bool approximatelyEqual(
    float a,
    float b,
    float epsilon = 0.1251f
)
{
    return std::fabs(a - b) <= epsilon;
}

void testMessageIds()
{
    CHECK(toMessageId(MessageType::CreateEntity) == 0x10);
    CHECK(toMessageId(MessageType::DestroyEntity) == 0x11);
    CHECK(toMessageId(MessageType::Snapshot) == 0x12);

    std::cout << "[OK] Message IDs\n";
}

void testPositionQuantization()
{
    std::uint16_t encoded = 0;

    CHECK(encodePosition(0.0f, encoded));
    CHECK(encoded == 32768);
    CHECK(decodePosition(encoded) == 0.0f);

    CHECK(encodePosition(120.25f, encoded));
    CHECK(encoded == 33249);
    CHECK(decodePosition(encoded) == 120.25f);

    CHECK(encodePosition(-40.5f, encoded));
    CHECK(encoded == 32606);
    CHECK(decodePosition(encoded) == -40.5f);

    CHECK(encodePosition(MIN_POSITION, encoded));
    CHECK(encoded == 0);
    CHECK(decodePosition(encoded) == MIN_POSITION);

    CHECK(encodePosition(MAX_POSITION, encoded));
    CHECK(encoded == 65535);
    CHECK(decodePosition(encoded) == MAX_POSITION);

    CHECK(encodePosition(123.1f, encoded));
    CHECK(approximatelyEqual(decodePosition(encoded), 123.1f));

    CHECK(!encodePosition(MIN_POSITION - 0.25f, encoded));
    CHECK(!encodePosition(MAX_POSITION + 0.25f, encoded));

    CHECK(
        !encodePosition(
            std::numeric_limits<float>::quiet_NaN(),
            encoded
        )
    );

    CHECK(
        !encodePosition(
            std::numeric_limits<float>::infinity(),
            encoded
        )
    );

    std::cout << "[OK] Position quantization\n";
}

void testCreateEntity()
{
    CreateEntityPayload original;

    original.entityId = 42;
    original.blueprintId = BlueprintId::EnemyBidos;
    original.x = 100.0f;
    original.y = -25.0f;

    PacketWriter writer;

    CHECK(original.serialize(writer));
    CHECK(writer.getSize() == CreateEntityPayload::SIZE);

    const std::vector<std::uint8_t> expected = {
        0x00, 0x00, 0x00, 0x2A,
        0x00, 0x03,
        0x81, 0x90,
        0x7F, 0x9C
    };
std::cout << "Expected: ";

for (const auto byte : expected) {
    std::cout << std::hex
              << static_cast<unsigned>(byte)
              << ' ';
}

std::cout << "\nActual:   ";

for (const auto byte : writer.getData()) {
    std::cout << std::hex
              << static_cast<unsigned>(byte)
              << ' ';
}

std::cout << std::dec << '\n';

    CHECK(writer.getData() == expected);

    PacketReader reader(writer.getData());

    CreateEntityPayload decoded;

    CHECK(decoded.deserialize(reader));
    CHECK(reader.isEnd());

    CHECK(decoded.entityId == 42);
    CHECK(decoded.blueprintId == BlueprintId::EnemyBidos);
    CHECK(decoded.x == 100.0f);
    CHECK(decoded.y == -25.0f);

    std::cout << "[OK] CreateEntity\n";
}

void testDestroyEntity()
{
    DestroyEntityPayload original;

    original.entityId = 0x12345678;

    PacketWriter writer;

    CHECK(original.serialize(writer));
    CHECK(writer.getSize() == DestroyEntityPayload::SIZE);

    const std::vector<std::uint8_t> expected = {
        0x12, 0x34, 0x56, 0x78
    };

    CHECK(writer.getData() == expected);

    PacketReader reader(writer.getData());

    DestroyEntityPayload decoded;

    CHECK(decoded.deserialize(reader));
    CHECK(reader.isEnd());
    CHECK(decoded.entityId == original.entityId);

    std::cout << "[OK] DestroyEntity\n";
}

void testEmptySnapshot()
{
    SnapshotPayload original;

    original.serverTick = 123;

    PacketWriter writer;

    CHECK(original.serialize(writer));
    CHECK(writer.getSize() == SnapshotPayload::HEADER_SIZE);

    const std::vector<std::uint8_t> expected = {
        0x00, 0x00, 0x00, 0x7B,
        0x00
    };

    CHECK(writer.getData() == expected);

    PacketReader reader(writer.getData());

    SnapshotPayload decoded;

    CHECK(decoded.deserialize(reader));
    CHECK(reader.isEnd());
    CHECK(decoded.serverTick == 123);
    CHECK(decoded.entities.empty());

    std::cout << "[OK] Empty snapshot\n";
}

void testSnapshot()
{
    SnapshotPayload original;

    original.serverTick = 60;

    original.entities = {
        {1, 100.0f, 200.0f},
        {2, -10.0f, 20.5f}
    };

    PacketWriter writer;

    CHECK(original.serialize(writer));

    CHECK(
        writer.getSize() ==
        SnapshotPayload::HEADER_SIZE +
        2 * SnapshotEntityState::SIZE
    );

    const std::vector<std::uint8_t> expected = {
        0x00, 0x00, 0x00, 0x3C,
        0x02,

        0x00, 0x00, 0x00, 0x01,
        0x81, 0x90,
        0x83, 0x20,

        0x00, 0x00, 0x00, 0x02,
        0x7F, 0xD8,
        0x80, 0x52
    };

    CHECK(writer.getData() == expected);

    PacketReader reader(writer.getData());

    SnapshotPayload decoded;

    CHECK(decoded.deserialize(reader));
    CHECK(reader.isEnd());
    CHECK(decoded.serverTick == 60);
    CHECK(decoded.entities.size() == 2);

    CHECK(decoded.entities[0].entityId == 1);
    CHECK(decoded.entities[0].x == 100.0f);
    CHECK(decoded.entities[0].y == 200.0f);

    CHECK(decoded.entities[1].entityId == 2);
    CHECK(decoded.entities[1].x == -10.0f);
    CHECK(decoded.entities[1].y == 20.5f);

    std::cout << "[OK] Snapshot\n";
}

void testMaximumSnapshot()
{
    static_assert(SnapshotEntityState::SIZE == 8);
    static_assert(SnapshotPayload::MAX_ENTITIES == 65);

    SnapshotPayload original;

    original.serverTick = 1000;

    for (
        std::uint32_t id = 1;
        id <= SnapshotPayload::MAX_ENTITIES;
        ++id
    ) {
        original.entities.push_back({
            id,
            static_cast<float>(id),
            -static_cast<float>(id)
        });
    }

    PacketWriter writer;

    MessageHeader header;

    header.setType(toMessageId(MessageType::Snapshot));

    header.size = static_cast<std::uint16_t>(
        original.serializedSize()
    );

    CHECK(header.serialize(writer));
    CHECK(original.serialize(writer));

    CHECK(writer.getSize() == 529);
    CHECK(writer.getSize() <= MAX_MESSAGE_SIZE);

    PacketReader reader(writer.getData());

    MessageHeader decodedHeader;

    CHECK(decodedHeader.deserialize(reader));

    CHECK(
        decodedHeader.getType() ==
        toMessageId(MessageType::Snapshot)
    );

    CHECK(
        decodedHeader.size ==
        original.serializedSize()
    );

    SnapshotPayload decoded;

    CHECK(decoded.deserialize(reader));
    CHECK(reader.isEnd());
    CHECK(decoded.serverTick == 1000);
    CHECK(decoded.entities.size() == SnapshotPayload::MAX_ENTITIES);

    for (std::size_t i = 0; i < decoded.entities.size(); ++i) {
        const float expected = static_cast<float>(i + 1);

        CHECK(decoded.entities[i].entityId == i + 1);
        CHECK(decoded.entities[i].x == expected);
        CHECK(decoded.entities[i].y == -expected);
    }

    std::cout << "[OK] Maximum snapshot\n";
}

void testOversizedSnapshot()
{
    SnapshotPayload snapshot;

    for (
        std::uint32_t id = 1;
        id <= SnapshotPayload::MAX_ENTITIES + 1;
        ++id
    ) {
        snapshot.entities.push_back({
            id,
            0.0f,
            0.0f
        });
    }

    PacketWriter writer;

    CHECK(!snapshot.serialize(writer));
    CHECK(writer.empty());

    std::cout << "[OK] Oversized snapshot rejected\n";
}

void testInvalidIds()
{
    CreateEntityPayload create;

    create.entityId = 0;

    PacketWriter writer1;

    CHECK(!create.serialize(writer1));
    CHECK(writer1.empty());

    DestroyEntityPayload destroy;

    destroy.entityId = 0;

    PacketWriter writer2;

    CHECK(!destroy.serialize(writer2));
    CHECK(writer2.empty());

    SnapshotPayload snapshot;

    snapshot.entities.push_back({
        0,
        10.0f,
        20.0f
    });

    PacketWriter writer3;

    CHECK(!snapshot.serialize(writer3));
    CHECK(writer3.empty());

    std::cout << "[OK] Invalid IDs rejected\n";
}

void testInvalidBlueprint()
{
    CreateEntityPayload create;

    create.entityId = 1;
    create.blueprintId = static_cast<BlueprintId>(999);

    PacketWriter writer;

    CHECK(!create.serialize(writer));
    CHECK(writer.empty());

    const std::vector<std::uint8_t> bytes = {
        0x00, 0x00, 0x00, 0x01,
        0xFF, 0xFF,
        0x80, 0x00,
        0x80, 0x00
    };

    PacketReader reader(bytes);

    CreateEntityPayload decoded;

    CHECK(!decoded.deserialize(reader));

    std::cout << "[OK] Invalid blueprint rejected\n";
}

void testDuplicateIds()
{
    SnapshotPayload snapshot;

    snapshot.entities = {
        {7, 10.0f, 20.0f},
        {7, 30.0f, 40.0f}
    };

    PacketWriter writer;

    CHECK(!snapshot.serialize(writer));
    CHECK(writer.empty());

    const std::vector<std::uint8_t> bytes = {
        0x00, 0x00, 0x00, 0x01,
        0x02,

        0x00, 0x00, 0x00, 0x07,
        0x80, 0x28,
        0x80, 0x50,

        0x00, 0x00, 0x00, 0x07,
        0x80, 0x78,
        0x80, 0xA0
    };

    PacketReader reader(bytes);

    SnapshotPayload decoded;

    CHECK(!decoded.deserialize(reader));

    std::cout << "[OK] Duplicate IDs rejected\n";
}

void testTruncatedPayloads()
{
    const std::vector<std::uint8_t> shortCreate = {
        0x00, 0x00, 0x00, 0x01
    };

    PacketReader createReader(shortCreate);

    CreateEntityPayload create;

    CHECK(!create.deserialize(createReader));

    const std::vector<std::uint8_t> shortDestroy = {
        0x00, 0x00, 0x00
    };

    PacketReader destroyReader(shortDestroy);

    DestroyEntityPayload destroy;

    CHECK(!destroy.deserialize(destroyReader));

    const std::vector<std::uint8_t> shortSnapshot = {
        0x00, 0x00, 0x00, 0x01,
        0x01
    };

    PacketReader snapshotReader(shortSnapshot);

    SnapshotPayload snapshot;

    CHECK(!snapshot.deserialize(snapshotReader));

    std::cout << "[OK] Truncated payloads rejected\n";
}

void testInvalidPositions()
{
    CreateEntityPayload create;

    create.entityId = 1;
    create.x = MIN_POSITION - 1.0f;

    PacketWriter writer;

    CHECK(!create.serialize(writer));
    CHECK(writer.empty());

    create.x = 0.0f;
    create.y = MAX_POSITION + 1.0f;

    PacketWriter secondWriter;

    CHECK(!create.serialize(secondWriter));
    CHECK(secondWriter.empty());

    SnapshotPayload snapshot;

    snapshot.entities.push_back({
        1,
        std::numeric_limits<float>::infinity(),
        0.0f
    });

    PacketWriter snapshotWriter;

    CHECK(!snapshot.serialize(snapshotWriter));
    CHECK(snapshotWriter.empty());

    std::cout << "[OK] Invalid positions rejected\n";
}

void testQuantizationPrecision()
{
    CreateEntityPayload original;

    original.entityId = 100;
    original.blueprintId = BlueprintId::Player;
    original.x = 123.1f;
    original.y = -456.3f;

    PacketWriter writer;

    CHECK(original.serialize(writer));

    PacketReader reader(writer.getData());

    CreateEntityPayload decoded;

    CHECK(decoded.deserialize(reader));
    CHECK(reader.isEnd());

    CHECK(approximatelyEqual(decoded.x, original.x));
    CHECK(approximatelyEqual(decoded.y, original.y));

    std::cout << "[OK] Quantization precision\n";
}

}

int main()
{
    testMessageIds();
    testPositionQuantization();
    testCreateEntity();
    testDestroyEntity();
    testEmptySnapshot();
    testSnapshot();
    testMaximumSnapshot();
    testOversizedSnapshot();
    testInvalidIds();
    testInvalidBlueprint();
    testDuplicateIds();
    testTruncatedPayloads();
    testInvalidPositions();
    testQuantizationPrecision();

    std::cout << "\nReplication payload tests: OK\n";

    return EXIT_SUCCESS;
}
