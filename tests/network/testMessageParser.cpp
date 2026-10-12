#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

#include "protocol/MessageHeader.hpp"
#include "protocol/MessageParser.hpp"
#include "protocol/PacketReader.hpp"
#include "protocol/PacketWriter.hpp"

static void testMultipleMessages()
{
    PacketWriter writer;


    MessageHeader inputHeader;

    inputHeader.setType(
        MessageType::Input
    );

    inputHeader.size = 3;

    assert(
        inputHeader.serialize(writer)
    );

    const std::uint8_t inputPayload[] = {
        0xAA,
        0xBB,
        0xCC
    };

    assert(
        writer.writeBytes(
            inputPayload,
            sizeof(inputPayload)
        )
    );


    MessageHeader pingHeader;

    pingHeader.setType(
        MessageType::Ping
    );

    pingHeader.size = 4;

    assert(
        pingHeader.serialize(writer)
    );

    const std::uint8_t pingPayload[] = {
        0x01,
        0x02,
        0x03,
        0x04
    };

    assert(
        writer.writeBytes(
            pingPayload,
            sizeof(pingPayload)
        )
    );


    PacketReader reader(
        writer.getData()
    );

    std::vector<ParsedMessage> messages;

    assert(
        MessageParser::parse(
            reader,
            messages
        )
    );

    assert(messages.size() == 2);

    assert(
        messages[0].type
        == MessageType::Input
    );

    assert(
        messages[0].payload.size()
        == 3
    );

    assert(
        messages[0].payload[0]
        == 0xAA
    );

    assert(
        messages[0].payload[1]
        == 0xBB
    );

    assert(
        messages[0].payload[2]
        == 0xCC
    );

    assert(
        messages[1].type
        == MessageType::Ping
    );

    assert(
        messages[1].payload.size()
        == 4
    );
}

static void testTruncatedHeader()
{
    const std::vector<std::uint8_t> data = {
        0x00,
        0x01
    };

    PacketReader reader(data);

    std::vector<ParsedMessage> messages;

    assert(
        !MessageParser::parse(
            reader,
            messages
        )
    );

    assert(messages.empty());
}

static void testInvalidPayloadSize()
{
    PacketWriter writer;

    MessageHeader header;

    header.setType(
        MessageType::Input
    );

    header.size = 10;

    assert(header.serialize(writer));

    const std::uint8_t payload[] = {
        0xAA,
        0xBB
    };

    assert(
        writer.writeBytes(
            payload,
            sizeof(payload)
        )
    );

    PacketReader reader(
        writer.getData()
    );

    std::vector<ParsedMessage> messages;

    assert(
        !MessageParser::parse(
            reader,
            messages
        )
    );

    assert(messages.empty());
}

static void testEmptyPayload()
{
    PacketWriter writer;

    MessageHeader header;

    header.setType(
        MessageType::Ping
    );

    header.size = 0;

    assert(header.serialize(writer));

    PacketReader reader(
        writer.getData()
    );

    std::vector<ParsedMessage> messages;

    assert(
        MessageParser::parse(
            reader,
            messages
        )
    );

    assert(messages.size() == 1);
    assert(messages[0].payload.empty());
}

static void testUnknownMessage()
{
    PacketWriter writer;

    MessageHeader header;

    header.setType(
        static_cast<MessageType>(999)
    );

    header.size = 2;

    assert(header.serialize(writer));

    const std::uint8_t payload[] = {
        0x42,
        0x43
    };

    assert(
        writer.writeBytes(
            payload,
            sizeof(payload)
        )
    );

    PacketReader reader(
        writer.getData()
    );

    std::vector<ParsedMessage> messages;

    assert(
        MessageParser::parse(
            reader,
            messages
        )
    );

    assert(messages.size() == 1);

    assert(
        static_cast<std::uint16_t>(
            messages[0].type
        ) == 999
    );
}

int main()
{
    testMultipleMessages();
    testTruncatedHeader();
    testInvalidPayloadSize();
    testEmptyPayload();
    testUnknownMessage();

    std::cout
        << "MessageParser tests: OK\n";

    return 0;
}
