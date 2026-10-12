#include <cassert>
#include <iostream>

#include "connection/Connection.hpp"

int main()
{
    std::cout << "Connection tests...\n";

    asio::ip::udp::endpoint endpoint(
        asio::ip::make_address("127.0.0.1"),
        4242
    );

    Connection connection(42, endpoint);

    assert(connection.getId() == 42);
    assert(connection.getEndpoint().port() == 4242);

    {
        PacketHeader header = connection.makeHeader();

        assert(header.connectionId == 42);
        assert(header.sequence == 1);
        assert(header.ack == 0);
        assert(header.ackBits == 0);
    }

    {
        PacketHeader header = connection.makeHeader();

        assert(header.sequence == 2);
    }

    {
        PacketHeader received{};
        received.connectionId = 42;
        received.sequence = 10;

        assert(
            connection.onPacketReceived(received)
            == PacketReceiveRes::New
        );
    }

    {
        PacketHeader header = connection.makeHeader();

        assert(header.connectionId == 42);
        assert(header.sequence == 3);

        assert(header.ack == 10);
        assert(header.ackBits == 0);
    }

    {
        PacketHeader received{};
        received.connectionId = 42;
        received.sequence = 11;

        connection.onPacketReceived(received);

        PacketHeader header = connection.makeHeader();

        assert(header.sequence == 4);
        assert(header.ack == 11);
        assert(header.ackBits == 0b1);
    }

    std::cout << "Connection tests: OK\n";

    return 0;
}
