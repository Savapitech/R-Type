#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

#include <asio.hpp>

#include "server/NetworkServer.hpp"
#include "protocol/HandshakeHeader.hpp"
#include "protocol/PacketReader.hpp"
#include "protocol/PacketWriter.hpp"
#include "config/NetworkConfig.hpp"

int main()
{
    constexpr std::uint16_t PORT = 4242;

    std::cout << "Starting NetworkServer test...\n";

    NetworkServer server(PORT);
    server.start();

    std::this_thread::sleep_for(
        std::chrono::milliseconds(50)
    );

    asio::io_context clientIo;

    asio::ip::udp::socket clientSocket(
        clientIo,
        asio::ip::udp::v4()
    );

    clientSocket.bind(
        asio::ip::udp::endpoint(
            asio::ip::udp::v4(),
            0
        )
    );

    asio::ip::udp::endpoint serverEndpoint(
        asio::ip::make_address("127.0.0.1"),
        PORT
    );


    PacketWriter writer;

    HandshakeHeader hello;
    hello.magic = MAGIC;
    hello.version = VERSION;
    hello.type = HandshakeType::Hello;

    assert(hello.serialize(writer));

    std::cout << "Sending HELLO...\n";

    clientSocket.send_to(
        asio::buffer(writer.getData()),
        serverEndpoint
    );


    std::array<std::uint8_t, MAX_PAYLOAD_SIZE> buffer{};

    asio::ip::udp::endpoint sender;

    clientSocket.non_blocking(true);

    asio::error_code error;
    std::size_t received = 0;

    const auto deadline =
        std::chrono::steady_clock::now()
        + std::chrono::seconds(2);

    while (
        std::chrono::steady_clock::now()
        < deadline
    ) {
        received = clientSocket.receive_from(
            asio::buffer(buffer),
            sender,
            0,
            error
        );

        if (!error)
            break;

        if (
            error != asio::error::would_block &&
            error != asio::error::try_again
        ) {
            std::cerr
                << "Receive error: "
                << error.message()
                << '\n';

            assert(false);
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(10)
        );
    }

    assert(received > 0);

    std::cout
        << "Received "
        << received
        << " bytes\n";


    PacketReader reader(
        buffer.data(),
        received
    );

    HandshakeHeader welcome;

    assert(welcome.deserialize(reader));

    assert(welcome.magic == MAGIC);
    assert(welcome.version == VERSION);
    assert(welcome.type == HandshakeType::Welcome);

    std::uint32_t connectionId = 0;

    assert(reader.readUInt32(connectionId));

    assert(connectionId != 0);

    std::cout
        << "WELCOME received\n"
        << "Connection ID: "
        << connectionId
        << '\n';

    server.stop();

    std::cout
        << "NetworkServer handshake test: OK\n";

    return 0;
}
