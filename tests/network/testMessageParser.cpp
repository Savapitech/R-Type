#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "protocol/MessageHeader.hpp"
#include "protocol/MessageParser.hpp"
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

static void testMultipleMessages()
{
    PacketWriter writer;

    MessageHeader firstHeader;

    firstHeader.setType(0x1001);
    firstHeader.size = 3;

    CHECK(firstHeader.serialize(writer));

    const std::uint8_t firstPayload[] = {
        0xAA, 0xBB, 0xCC
    };

    CHECK(
        writer.writeBytes(
            firstPayload,
            sizeof(firstPayload)
        )
    );

    MessageHeader secondHeader;

    secondHeader.setType(0x2002);
    secondHeader.size = 4;

    CHECK(secondHeader.serialize(writer));

    const std::uint8_t secondPayload[] = {
        0x01, 0x02, 0x03, 0x04
    };

    CHECK(
        writer.writeBytes(
            secondPayload,
            sizeof(secondPayload)
        )
    );

    PacketReader reader(writer.getData());

    std::vector<ParsedMessage> messages;

    CHECK(MessageParser::parse(reader, messages));
    CHECK(reader.isEnd());
    CHECK(messages.size() == 2);

    CHECK(messages[0].type == 0x1001);
    CHECK(
        messages[0].payload ==
        std::vector<std::uint8_t>({0xAA, 0xBB, 0xCC})
    );

    CHECK(messages[1].type == 0x2002);
    CHECK(
        messages[1].payload ==
        std::vector<std::uint8_t>({0x01, 0x02, 0x03, 0x04})
    );
}

static void testTruncatedHeader()
{
    const std::vector<std::uint8_t> data = {
        0x00, 0x01
    };

    PacketReader reader(data);

    std::vector<ParsedMessage> messages;

    CHECK(!MessageParser::parse(reader, messages));
    CHECK(messages.empty());
}

static void testInvalidPayloadSize()
{
    PacketWriter writer;

    MessageHeader header;

    header.setType(0x1001);
    header.size = 10;

    CHECK(header.serialize(writer));

    const std::uint8_t payload[] = {
        0xAA, 0xBB
    };

    CHECK(
        writer.writeBytes(
            payload,
            sizeof(payload)
        )
    );

    PacketReader reader(writer.getData());

    std::vector<ParsedMessage> messages;

    CHECK(!MessageParser::parse(reader, messages));
    CHECK(messages.empty());
}

static void testEmptyPayload()
{
    PacketWriter writer;

    MessageHeader header;

    header.setType(0x2002);
    header.size = 0;

    CHECK(header.serialize(writer));

    PacketReader reader(writer.getData());

    std::vector<ParsedMessage> messages;

    CHECK(MessageParser::parse(reader, messages));
    CHECK(reader.isEnd());
    CHECK(messages.size() == 1);
    CHECK(messages[0].type == 0x2002);
    CHECK(messages[0].payload.empty());
}

static void testUnknownMessage()
{
    PacketWriter writer;

    MessageHeader header;

    header.setType(999);
    header.size = 2;

    CHECK(header.serialize(writer));

    const std::uint8_t payload[] = {
        0x42, 0x43
    };

    CHECK(
        writer.writeBytes(
            payload,
            sizeof(payload)
        )
    );

    PacketReader reader(writer.getData());

    std::vector<ParsedMessage> messages;

    CHECK(MessageParser::parse(reader, messages));
    CHECK(reader.isEnd());
    CHECK(messages.size() == 1);
    CHECK(messages[0].type == 999);

    CHECK(
        messages[0].payload ==
        std::vector<std::uint8_t>({0x42, 0x43})
    );
}

static void testEmptyDatagram()
{
    const std::vector<std::uint8_t> data;

    PacketReader reader(data);

    std::vector<ParsedMessage> messages;

    CHECK(MessageParser::parse(reader, messages));
    CHECK(messages.empty());
    CHECK(reader.isEnd());
}

int main()
{
    testMultipleMessages();
    testTruncatedHeader();
    testInvalidPayloadSize();
    testEmptyPayload();
    testUnknownMessage();
    testEmptyDatagram();

    std::cout << "MessageParser tests: OK\n";

    return EXIT_SUCCESS;
}
