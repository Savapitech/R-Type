#include "Server.hpp"

#include "Protocol/MessageType.hpp"
#include "Protocol/PayloadCodec.hpp"

#include "protocol/HandshakeHeader.hpp"
#include "protocol/MessageHeader.hpp"
#include "protocol/PacketHeader.hpp"
#include "protocol/PacketReader.hpp"
#include "protocol/PacketWriter.hpp"

#include <asio.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

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

using udp = asio::ip::udp;

constexpr std::uint16_t SERVER_PORT = 42430;

std::vector<std::uint8_t> receiveDatagram(
    udp::socket &socket,
    std::chrono::milliseconds timeout
)
{
    std::array<std::uint8_t, 2048> buffer{};
    udp::endpoint sender;

    asio::error_code error;

    socket.non_blocking(true, error);
    CHECK(!error);

    const auto deadline =
        std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline) {
        error.clear();

        const std::size_t received = socket.receive_from(
            asio::buffer(buffer),
            sender,
            0,
            error
        );

        if (!error) {
            return {
                buffer.begin(),
                buffer.begin() + received
            };
        }

        if (
            error != asio::error::would_block &&
            error != asio::error::try_again
        ) {
            std::cerr
                << "UDP receive error: "
                << error.message()
                << '\n';

            return {};
        }

        std::this_thread::sleep_for(1ms);
    }

    return {};
}

void sendHello(
    udp::socket &socket,
    const udp::endpoint &serverEndpoint
)
{
    PacketWriter writer;

    HandshakeHeader hello;

    hello.magic = MAGIC;
    hello.version = VERSION;
    hello.type = HandshakeType::Hello;

    CHECK(hello.serialize(writer));

    const auto sent = socket.send_to(
        asio::buffer(writer.getData()),
        serverEndpoint
    );

    CHECK(sent == writer.getSize());
}

std::uint32_t receiveWelcome(udp::socket &socket)
{
    const auto data = receiveDatagram(socket, 2s);

    CHECK(!data.empty());

    PacketReader reader(data);

    HandshakeHeader welcome;

    CHECK(welcome.deserialize(reader));
    CHECK(welcome.magic == MAGIC);
    CHECK(welcome.version == VERSION);
    CHECK(welcome.type == HandshakeType::Welcome);

    std::uint32_t connectionId = 0;

    CHECK(reader.readUInt32(connectionId));
    CHECK(reader.isEnd());
    CHECK(connectionId != 0);

    return connectionId;
}

void sendInput(
    udp::socket &socket,
    const udp::endpoint &serverEndpoint,
    std::uint32_t connectionId,
    std::uint32_t packetSequence,
    std::uint32_t inputSequence,
    std::uint32_t clientTick,
    rtype::protocol::InputAction actions
)
{
    PacketWriter writer;

    PacketHeader packetHeader{};

    packetHeader.connectionId = connectionId;
    packetHeader.sequence = packetSequence;
    packetHeader.ack = 0;
    packetHeader.ackBits = 0;

    CHECK(packetHeader.serialize(writer));

    MessageHeader messageHeader;

    messageHeader.setType(
        rtype::protocol::toMessageId(
            rtype::protocol::MessageType::Input
        )
    );

    messageHeader.size =
        static_cast<std::uint16_t>(
            sizeof(rtype::protocol::InputPayload)
        );

    CHECK(messageHeader.serialize(writer));

    const rtype::protocol::InputPayload input{
        rtype::protocol::be32(inputSequence),
        rtype::protocol::be32(clientTick),
        actions
    };

    CHECK(rtype::protocol::writePayload(writer, input));

    const auto sent = socket.send_to(
        asio::buffer(writer.getData()),
        serverEndpoint
    );

    CHECK(sent == writer.getSize());
}

bool waitForInput(
    rtype::server::Server &server,
    rtype::server::Server::InputEvent &event,
    std::chrono::milliseconds timeout
)
{
    const auto deadline =
        std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline) {
        if (server.pollInput(event))
            return true;

        std::this_thread::sleep_for(1ms);
    }

    return false;
}

void testInputQueue()
{
    rtype::server::Server server(SERVER_PORT);

    server.start();

    asio::io_context io;

    udp::socket socket(
        io,
        udp::endpoint(udp::v4(), 0)
    );

    const udp::endpoint serverEndpoint(
        asio::ip::make_address("127.0.0.1"),
        SERVER_PORT
    );

    sendHello(socket, serverEndpoint);

    const std::uint32_t connectionId =
        receiveWelcome(socket);

    sendInput(
        socket,
        serverEndpoint,
        connectionId,
        1,
        1,
        100,
        rtype::protocol::InputAction::Right
    );

    std::this_thread::sleep_for(10ms);

    sendInput(
        socket,
        serverEndpoint,
        connectionId,
        2,
        3,
        101,
        rtype::protocol::InputAction::Shoot
    );

    std::this_thread::sleep_for(10ms);

    sendInput(
        socket,
        serverEndpoint,
        connectionId,
        3,
        2,
        102,
        rtype::protocol::InputAction::Left
    );

    std::this_thread::sleep_for(10ms);

    sendInput(
        socket,
        serverEndpoint,
        connectionId,
        4,
        3,
        103,
        rtype::protocol::InputAction::Up
    );

    std::this_thread::sleep_for(10ms);

    sendInput(
        socket,
        serverEndpoint,
        connectionId,
        5,
        4,
        104,
        rtype::protocol::InputAction::Right |
            rtype::protocol::InputAction::Shoot
    );

    rtype::server::Server::InputEvent event;

    CHECK(waitForInput(server, event, 2s));
    CHECK(event.connectionId == connectionId);
    CHECK(event.input.inputSequence.get() == 1);
    CHECK(event.input.clientTick.get() == 100);

    CHECK(
        event.input.actions ==
        rtype::protocol::InputAction::Right
    );

    CHECK(waitForInput(server, event, 2s));
    CHECK(event.connectionId == connectionId);
    CHECK(event.input.inputSequence.get() == 3);
    CHECK(event.input.clientTick.get() == 101);

    CHECK(
        event.input.actions ==
        rtype::protocol::InputAction::Shoot
    );

    CHECK(waitForInput(server, event, 2s));
    CHECK(event.connectionId == connectionId);
    CHECK(event.input.inputSequence.get() == 4);
    CHECK(event.input.clientTick.get() == 104);

    CHECK(
        rtype::protocol::hasAction(
            event.input.actions,
            rtype::protocol::InputAction::Right
        )
    );

    CHECK(
        rtype::protocol::hasAction(
            event.input.actions,
            rtype::protocol::InputAction::Shoot
        )
    );

    std::this_thread::sleep_for(100ms);

    CHECK(!server.pollInput(event));

    server.stop();

    std::cout << "InputQueue integration test: OK\n";
}

}

int main()
{
    testInputQueue();

    return EXIT_SUCCESS;
}
