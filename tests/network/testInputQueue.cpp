#include "Server.hpp"

#include "Protocol/PayloadCodec.hpp"

#include "protocol/HandshakeHeader.hpp"
#include "protocol/MessageHeader.hpp"
#include "protocol/PacketHeader.hpp"
#include "protocol/PacketReader.hpp"
#include "protocol/PacketWriter.hpp"

#include <asio.hpp>

#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

namespace {

using udp = asio::ip::udp;

constexpr std::uint16_t SERVER_PORT = 42430;

using namespace std::chrono_literals;

/**
 * Attend un datagramme UDP pendant un temps maximum.
 *
 * Retourne un vector vide si rien n'est reçu avant le timeout.
 */
std::vector<std::uint8_t>
receiveDatagram(udp::socket &socket, std::chrono::milliseconds timeout)
{
    std::array<std::uint8_t, 2048> buffer{};
    udp::endpoint sender;

    asio::error_code error;

    socket.non_blocking(true, error);
    assert(!error);

    const auto deadline =
        std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline) {
        error.clear();

        const std::size_t received =
            socket.receive_from(
                asio::buffer(buffer),
                sender,
                0,
                error
            );

        if (!error) {
            asio::error_code ignored;
            socket.non_blocking(false, ignored);

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

            break;
        }

        std::this_thread::sleep_for(1ms);
    }

    asio::error_code ignored;
    socket.non_blocking(false, ignored);

    return {};
}

/**
 * Envoie le HELLO au serveur.
 */
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

    assert(hello.serialize(writer));

    const std::size_t sent =
        socket.send_to(
            asio::buffer(writer.getData()),
            serverEndpoint
        );

    assert(sent == writer.getSize());
}

/**
 * Lit le WELCOME reçu du serveur et retourne le connectionId.
 */
std::uint32_t receiveWelcome(udp::socket &socket)
{
    const std::vector<std::uint8_t> data =
        receiveDatagram(socket, 1s);

    assert(!data.empty());

    PacketReader reader(data);

    HandshakeHeader welcome;

    assert(welcome.deserialize(reader));
    assert(welcome.magic == MAGIC);
    assert(welcome.version == VERSION);
    assert(welcome.type == HandshakeType::Welcome);

    std::uint32_t connectionId = 0;

    assert(reader.readUInt32(connectionId));
    assert(reader.isEnd());

    assert(connectionId != 0);

    return connectionId;
}

/**
 * Construit et envoie un packet connecté contenant un Input.
 *
 * packetSequence :
 *     séquence du PacketHeader
 *
 * inputSequence :
 *     séquence logique des inputs joueur
 *
 * Les deux sont volontairement différentes.
 */
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

    /*
     * Packet header
     */
    PacketHeader packetHeader;

    packetHeader.connectionId = connectionId;
    packetHeader.sequence = packetSequence;
    packetHeader.ack = 0;
    packetHeader.ackBits = 0;

    assert(packetHeader.serialize(writer));

    /*
     * Message header
     */
    MessageHeader messageHeader;

    messageHeader.setType(MessageType::Input);
    messageHeader.size =
        static_cast<std::uint16_t>(
            sizeof(rtype::protocol::InputPayload)
        );

    assert(messageHeader.serialize(writer));

    /*
     * Input payload
     */
    rtype::protocol::InputPayload input{
        rtype::protocol::be32(inputSequence),
        rtype::protocol::be32(clientTick),
        actions
    };

    assert(
        rtype::protocol::writePayload(
            writer,
            input
        )
    );

    /*
     * Envoi UDP
     */
    const std::size_t sent =
        socket.send_to(
            asio::buffer(writer.getData()),
            serverEndpoint
        );

    assert(sent == writer.getSize());
}

/**
 * Attend qu'un InputEvent soit disponible dans Server::_inputQueue.
 */
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

} // namespace

int main()
{
    std::cout << "Input queue test..." << std::endl;

    /*
     * Démarrage du vrai serveur.
     */
    rtype::server::Server server(SERVER_PORT);

    server.start();

    /*
     * Socket représentant notre faux client.
     *
     * Port 0 = l'OS choisit automatiquement un port disponible.
     */
    asio::io_context io;

    udp::socket socket(
        io,
        udp::endpoint(
            udp::v4(),
            0
        )
    );

    const udp::endpoint serverEndpoint(
        asio::ip::make_address("127.0.0.1"),
        SERVER_PORT
    );

    /*
     * ------------------------------------------------------------
     * HANDSHAKE
     * ------------------------------------------------------------
     */

    sendHello(
        socket,
        serverEndpoint
    );

    const std::uint32_t connectionId =
        receiveWelcome(socket);

    std::cout
        << "Connected with ID "
        << connectionId
        << std::endl;

    /*
     * ------------------------------------------------------------
     * INPUTS
     * ------------------------------------------------------------
     *
     * Attention :
     *
     * Packet sequence : 1, 2, 3, 4, 5
     *
     * Input sequence  : 1, 3, 2, 3, 4
     *
     * On garde les PacketHeader.sequence dans l'ordre pour que
     * PacketTracker accepte bien les cinq packets.
     *
     * C'est Server qui doit ensuite filtrer inputSequence.
     */

    sendInput(
        socket,
        serverEndpoint,
        connectionId,
        1, // packet sequence
        1, // input sequence
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

    /*
     * Ancien input : doit être rejeté.
     */
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

    /*
     * Duplicate de seq=3 : doit être rejeté.
     */
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

    /*
     * Nouvel input : accepté.
     */
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

    /*
     * ------------------------------------------------------------
     * VÉRIFICATION DE LA QUEUE
     * ------------------------------------------------------------
     */

    rtype::server::Server::InputEvent event;

    /*
     * Premier événement : seq 1
     */
    assert(
        waitForInput(
            server,
            event,
            1s
        )
    );

    assert(event.connectionId == connectionId);
    assert(event.input.inputSequence.get() == 1);
    assert(event.input.clientTick.get() == 100);
    assert(
        event.input.actions ==
        rtype::protocol::InputAction::Right
    );

    /*
     * Deuxième événement : seq 3
     */
    assert(
        waitForInput(
            server,
            event,
            1s
        )
    );

    assert(event.connectionId == connectionId);
    assert(event.input.inputSequence.get() == 3);
    assert(event.input.clientTick.get() == 101);
    assert(
        event.input.actions ==
        rtype::protocol::InputAction::Shoot
    );

    /*
     * Troisième événement : seq 4
     *
     * seq 2 et le deuxième seq 3 ne doivent jamais apparaître.
     */
    assert(
        waitForInput(
            server,
            event,
            1s
        )
    );

    assert(event.connectionId == connectionId);
    assert(event.input.inputSequence.get() == 4);
    assert(event.input.clientTick.get() == 104);

    assert(
        rtype::protocol::hasAction(
            event.input.actions,
            rtype::protocol::InputAction::Right
        )
    );

    assert(
        rtype::protocol::hasAction(
            event.input.actions,
            rtype::protocol::InputAction::Shoot
        )
    );

    /*
     * Laisse au network thread le temps de terminer.
     *
     * La queue doit maintenant être vide.
     */
    std::this_thread::sleep_for(100ms);

    assert(!server.pollInput(event));

    server.stop();

    std::cout
        << "Input queue test: OK"
        << std::endl;

    return 0;
}
