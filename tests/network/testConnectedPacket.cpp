#include <asio.hpp>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <future>
#include <iostream>
#include <memory>
#include <vector>

#include "config/NetworkConfig.hpp"
#include "protocol/HandshakeHeader.hpp"
#include "protocol/MessageHeader.hpp"
#include "protocol/PacketHeader.hpp"
#include "protocol/PacketReader.hpp"
#include "protocol/PacketWriter.hpp"
#include "server/NetworkServer.hpp"

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

constexpr std::uint16_t PORT = 42433;
constexpr MessageId MESSAGE_ID = 0xBEEF;

struct ReceivedMessage {
    std::uint32_t connectionId = 0;
    MessageId type = 0;
    std::vector<std::uint8_t> payload;
};

std::vector<std::uint8_t> receiveDatagram(
    udp::socket &socket,
    std::chrono::milliseconds timeout
)
{
    std::vector<std::uint8_t> data(MAX_PAYLOAD_SIZE);
    udp::endpoint sender;

    asio::error_code error;

    socket.non_blocking(true, error);
    CHECK(!error);

    const auto deadline =
        std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline) {
        error.clear();

        const auto received = socket.receive_from(
            asio::buffer(data),
            sender,
            0,
            error
        );

        if (!error) {
            data.resize(received);
            return data;
        }

        if (
            error != asio::error::would_block &&
            error != asio::error::try_again
        ) {
            return {};
        }

        std::this_thread::sleep_for(1ms);
    }

    return {};
}

void testConnectedPacket()
{
    NetworkServer server(PORT);

    auto promise =
        std::make_shared<std::promise<ReceivedMessage>>();

    auto future = promise->get_future();

    server.setOnMessage(
        [promise](
            std::uint32_t connectionId,
            MessageId type,
            const std::vector<std::uint8_t> &payload
        ) {
            try {
                promise->set_value({
                    connectionId,
                    type,
                    payload
                });
            } catch (const std::future_error &) {
            }
        }
    );

    server.start();

    asio::io_context io;

    udp::socket socket(
        io,
        udp::endpoint(udp::v4(), 0)
    );

    const udp::endpoint serverEndpoint(
        asio::ip::make_address("127.0.0.1"),
        PORT
    );

    PacketWriter helloWriter;

    HandshakeHeader hello;

    hello.magic = MAGIC;
    hello.version = VERSION;
    hello.type = HandshakeType::Hello;

    CHECK(hello.serialize(helloWriter));

    const auto helloSent = socket.send_to(
        asio::buffer(helloWriter.getData()),
        serverEndpoint
    );

    CHECK(helloSent == helloWriter.getSize());

    const auto welcomeData = receiveDatagram(
        socket,
        2s
    );

    CHECK(!welcomeData.empty());

    PacketReader welcomeReader(welcomeData);

    HandshakeHeader welcome;

    CHECK(welcome.deserialize(welcomeReader));
    CHECK(welcome.magic == MAGIC);
    CHECK(welcome.version == VERSION);
    CHECK(welcome.type == HandshakeType::Welcome);

    std::uint32_t connectionId = 0;

    CHECK(welcomeReader.readUInt32(connectionId));
    CHECK(welcomeReader.isEnd());
    CHECK(connectionId != 0);

    PacketWriter packetWriter;

    PacketHeader packetHeader{};

    packetHeader.connectionId = connectionId;
    packetHeader.sequence = 1;
    packetHeader.ack = 0;
    packetHeader.ackBits = 0;

    CHECK(packetHeader.serialize(packetWriter));

    const std::vector<std::uint8_t> payload = {
        0xDE, 0xAD, 0xBE, 0xEF
    };

    MessageHeader messageHeader;

    messageHeader.setType(MESSAGE_ID);
    messageHeader.size =
        static_cast<std::uint16_t>(payload.size());

    CHECK(messageHeader.serialize(packetWriter));

    CHECK(
        packetWriter.writeBytes(
            payload.data(),
            payload.size()
        )
    );

    const auto packetSent = socket.send_to(
        asio::buffer(packetWriter.getData()),
        serverEndpoint
    );

    CHECK(packetSent == packetWriter.getSize());

    CHECK(future.wait_for(2s) == std::future_status::ready);

    const ReceivedMessage received = future.get();

    CHECK(received.connectionId == connectionId);
    CHECK(received.type == MESSAGE_ID);
    CHECK(received.payload == payload);

    server.stop();

    std::cout << "ConnectedPacket integration test: OK\n";
}

}

int main()
{
    testConnectedPacket();

    return EXIT_SUCCESS;
}
