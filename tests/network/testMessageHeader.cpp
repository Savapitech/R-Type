#include <cassert>
#include <cstdint>
#include <iostream>

#include "protocol/MessageHeader.hpp"
#include "protocol/PacketReader.hpp"
#include "protocol/PacketWriter.hpp"

int main()
{
    MessageHeader header;

    header.setType(MessageType::Input);
    header.size = 9;

    PacketWriter writer;

    assert(header.serialize(writer));

    assert(
        writer.getSize()
        == MessageHeader::HEADER_SIZE
    );

    const auto &data = writer.getData();

    /*
        type = 0x0001
        size = 0x0009

        wire:
        00 01 00 09
    */

    assert(data[0] == 0x00);
    assert(data[1] == 0x01);
    assert(data[2] == 0x00);
    assert(data[3] == 0x09);

    PacketReader reader(data);

    MessageHeader decoded;

    assert(decoded.deserialize(reader));

    assert(decoded.getType() == MessageType::Input);
    assert(decoded.size == 9);

    std::cout << "MessageHeader test: OK\n";

    return 0;
}
