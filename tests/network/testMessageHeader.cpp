#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "protocol/MessageHeader.hpp"
#include "protocol/PacketReader.hpp"
#include "protocol/PacketWriter.hpp"

#define CHECK(condition)                                      \
    do {                                                      \
        if (!(condition)) {                                   \
            std::cerr << "CHECK FAILED: " #condition          \
                      << " at " << __FILE__ << ":"             \
                      << __LINE__ << '\n';                    \
            std::exit(EXIT_FAILURE);                          \
        }                                                     \
    } while (false)

static void testMessageHeader()
{
    MessageHeader header;

    header.setType(0x1234);
    header.size = 9;

    PacketWriter writer;

    CHECK(header.serialize(writer));
    CHECK(writer.getSize() == MessageHeader::HEADER_SIZE);

    const std::vector<std::uint8_t> expected = {
        0x12, 0x34, 0x00, 0x09
    };

    CHECK(writer.getData() == expected);

    PacketReader reader(writer.getData());

    MessageHeader decoded;

    CHECK(decoded.deserialize(reader));
    CHECK(decoded.getType() == 0x1234);
    CHECK(decoded.size == 9);
    CHECK(reader.isEnd());
}

static void testZeroPayload()
{
    MessageHeader header;

    header.setType(0x0000);
    header.size = 0;

    PacketWriter writer;

    CHECK(header.serialize(writer));

    PacketReader reader(writer.getData());

    MessageHeader decoded;

    CHECK(decoded.deserialize(reader));
    CHECK(decoded.getType() == 0);
    CHECK(decoded.size == 0);
    CHECK(reader.isEnd());
}

static void testMaximumValues()
{
    MessageHeader header;

    header.setType(0xFFFF);
    header.size = 0xFFFF;

    PacketWriter writer;

    CHECK(header.serialize(writer));

    const std::vector<std::uint8_t> expected = {
        0xFF, 0xFF, 0xFF, 0xFF
    };

    CHECK(writer.getData() == expected);

    PacketReader reader(writer.getData());

    MessageHeader decoded;

    CHECK(decoded.deserialize(reader));
    CHECK(decoded.getType() == 0xFFFF);
    CHECK(decoded.size == 0xFFFF);
    CHECK(reader.isEnd());
}

static void testTruncatedHeader()
{
    const std::vector<std::uint8_t> data = {
        0x12, 0x34, 0x00
    };

    PacketReader reader(data);

    MessageHeader header;

    CHECK(!header.deserialize(reader));
}

int main()
{
    testMessageHeader();
    testZeroPayload();
    testMaximumValues();
    testTruncatedHeader();

    std::cout << "MessageHeader tests: OK\n";

    return EXIT_SUCCESS;
}
