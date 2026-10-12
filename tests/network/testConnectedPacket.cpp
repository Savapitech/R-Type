#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

#include <asio.hpp>

#include "server/NetworkServer.hpp"

#include "protocol/HandshakeHeader.hpp"
#include "protocol/MessageHeader.hpp"
#include "protocol/MessageParser.hpp"
#include "protocol/PacketHeader.hpp"
#include "protocol/PacketReader.hpp"
#include "protocol/PacketWriter.hpp"
#include "protocol/PingMessage.hpp"
#include "protocol/PongMessage.hpp"

#include "config/NetworkConfig.hpp"


static std::size_t receiveWithTimeout(
    asio::ip::udp::socket &socket,
    std::array<std::uint8_t, MAX_PAYLOAD_SIZE> &buffer,
    asio::ip::udp::endpoint &sender
)
{
    socket.non_blocking(true);

    const auto deadline =
        std::chrono::steady_clock::now()
        + std::chrono::seconds(2);

    while (
        std::chrono::steady_clock::now()
        < deadline
    ) {
        asio::error_code error;

        const std::size_t received =
            socket.receive_from(
                asio::buffer(buffer),
                sender,
                0,
                error
            );

        if (!error)
            return received;

        if (
            error != asio::error::would_block &&
            error != asio::error::try_again
        ) {
            std::cerr
                << "Receive error: "
                << error.message()
                << '\n';

            return 0;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(10)
        );
    }

    return 0;
}


int main()
{
    constexpr std::uint16_t PORT = 4242;
    constexpr std::uint32_t TIMESTAMP = 0x12345678;

    std::cout << "PING -> PONG integration test...\n";



    NetworkServer server(PORT);

    server.start();



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

    const asio::ip::udp::endpoint serverEndpoint(
        asio::ip::make_address("127.0.0.1"),
        PORT
    );



    PacketWriter helloWriter;

    HandshakeHeader hello;

    hello.magic = MAGIC;
    hello.version = VERSION;
    hello.type = HandshakeType::Hello;

    assert(
        hello.serialize(helloWriter)
    );

    clientSocket.send_to(
        asio::buffer(
            helloWriter.getData()
        ),
        serverEndpoint
    );

    std::cout << "HELLO sent\n";



    std::array<
        std::uint8_t,
        MAX_PAYLOAD_SIZE
    > receiveBuffer{};

    asio::ip::udp::endpoint sender;

    std::size_t received =
        receiveWithTimeout(
            clientSocket,
            receiveBuffer,
            sender
        );

    assert(received > 0);

    std::vector<std::uint8_t> welcomeData(
        receiveBuffer.begin(),
        receiveBuffer.begin() + received
    );

    PacketReader welcomeReader(
        welcomeData
    );

    HandshakeHeader welcome;

    assert(
        welcome.deserialize(
            welcomeReader
        )
    );

    assert(
        welcome.type
        == HandshakeType::Welcome
    );

    std::uint32_t connectionId = 0;

    assert(
        welcomeReader.readUInt32(
            connectionId
        )
    );

    assert(connectionId != 0);

    std::cout
        << "WELCOME received, connectionId="
        << connectionId
        << '\n';



    PacketWriter packetWriter;



    PacketHeader packetHeader{};

    packetHeader.connectionId =
        connectionId;

    packetHeader.sequence = 1;
    packetHeader.ack = 0;
    packetHeader.ackBits = 0;

    assert(
        packetHeader.serialize(
            packetWriter
        )
    );



    messageHeader.setType(
        MessageType::Ping
    );

    messageHeader.size =
        PingMessage::SIZE;

    assert(
        messageHeader.serialize(
            packetWriter
        )
    );



    PingMessage ping;

    ping.timestamp = TIMESTAMP;

    assert(
        ping.serialize(
            packetWriter
        )
    );



    clientSocket.send_to(
        asio::buffer(
            packetWriter.getData()
        ),
        serverEndpoint
    );

    std::cout
        << "PING sent, timestamp="
        << TIMESTAMP
        << '\n';



    receiveBuffer.fill(0);

    received =
        receiveWithTimeout(
            clientSocket,
            receiveBuffer,
            sender
        );

    assert(received > 0);

    std::cout
        << "Received "
        << received
        << " bytes from server\n";



    std::vector<std::uint8_t> responseData(
        receiveBuffer.begin(),
        receiveBuffer.begin() + received
    );

    PacketReader responseReader(
        responseData
    );



    PacketHeader responseHeader{};

    assert(
        responseHeader.deserialize(
            responseReader
        )
    );

    std::cout
        << "PacketHeader:"
        << " connectionId="
        << responseHeader.connectionId
        << " sequence="
        << responseHeader.sequence
        << " ack="
        << responseHeader.ack
        << " ackBits="
        << responseHeader.ackBits
        << '\n';



    assert(
        responseHeader.connectionId
        == connectionId
    );

    assert(
        responseHeader.sequence
        == 1
    );

    assert(
        responseHeader.ack
        == 1
    );

    assert(
        responseHeader.ackBits
        == 0
    );



    std::vector<ParsedMessage> messages;

    assert(
        MessageParser::parse(
            responseReader,
            messages
        )
    );

    assert(
        messages.size()
        == 1
    );



    const ParsedMessage &message =
        messages[0];

    assert(
        message.type
        == MessageType::Pong
    );

    assert(
        message.payload.size()
        == PongMessage::SIZE
    );



    PacketReader pongReader(
        message.payload
    );

    PongMessage pong;

    assert(
        pong.deserialize(
            pongReader
        )
    );

    assert(
        pongReader.isEnd()
    );



    assert(
        pong.timestamp
        == TIMESTAMP
    );

    std::cout
        << "PONG received, timestamp="
        << pong.timestamp
        << '\n';



    server.stop();

    std::cout
        << "PING -> PONG integration test: OK\n";

    return 0;
}
