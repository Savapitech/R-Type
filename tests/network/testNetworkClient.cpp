#include "NetworkClient.hpp"
#include "Server.hpp"

#include "Protocol/InputPayload.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

namespace {

constexpr std::uint16_t SERVER_PORT = 42431;

#define CHECK(condition)                                         \
    do {                                                         \
        if (!(condition)) {                                      \
            std::cerr                                            \
                << "CHECK FAILED: " #condition                   \
                << " at " << __FILE__                             \
                << ":" << __LINE__ << '\n';                      \
            std::exit(EXIT_FAILURE);                             \
        }                                                        \
    } while (false)

/**
 * Attend un événement donné pendant un temps limité.
 *
 * Évite un sleep fixe trop court ou une attente infinie.
 */
bool waitForEvent(
    rtype::client::NetworkClient &client,
    rtype::client::NetworkEventType expectedType,
    rtype::client::NetworkEvent &event,
    std::chrono::milliseconds timeout
)
{
    const auto deadline =
        std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline) {
        if (client.pollEvent(event)) {
            if (event.type == expectedType)
                return true;

            std::cerr
                << "Unexpected network event: "
                << static_cast<int>(event.type)
                << '\n';

            return false;
        }

        std::this_thread::sleep_for(1ms);
    }

    return false;
}

/**
 * Attend qu'un InputEvent soit disponible côté serveur.
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

void testConnection(
    rtype::client::NetworkClient &client,
    std::uint32_t &connectionId
)
{
    std::cout
        << "[TEST] Client connection..."
        << std::endl;

    CHECK(
        client.connect(
            "127.0.0.1",
            std::to_string(SERVER_PORT)
        )
    );

    CHECK(client.isRunning());
    CHECK(!client.isConnected());

    /*
     * Premier HELLO.
     */
    client.sendHello();

    /*
     * Attente du WELCOME.
     */
    rtype::client::NetworkEvent event;

    CHECK(
        waitForEvent(
            client,
            rtype::client::NetworkEventType::Connected,
            event,
            2s
        )
    );

    CHECK(client.isConnected());

    connectionId = client.getConnectionId();

    CHECK(connectionId != 0);

    CHECK(event.connectionId == connectionId);

    std::cout
        << "[OK] Connected with ID "
        << connectionId
        << std::endl;
}

void testInput(
    rtype::client::NetworkClient &client,
    rtype::server::Server &server,
    std::uint32_t connectionId
)
{
    std::cout
        << "[TEST] Sending Input..."
        << std::endl;

    constexpr std::uint32_t CLIENT_TICK = 42;

    const auto actions =
        rtype::protocol::InputAction::Right |
        rtype::protocol::InputAction::Shoot;

    client.sendInput(
        CLIENT_TICK,
        actions
    );

    /*
     * Le serveur doit recevoir l'Input
     * dans sa queue thread-safe.
     */
    rtype::server::Server::InputEvent event;

    CHECK(
        waitForInput(
            server,
            event,
            2s
        )
    );

    /*
     * Vérification de l'identité du client.
     */
    CHECK(event.connectionId == connectionId);

    /*
     * Premier input envoyé :
     * inputSequence commence à 1.
     */
    CHECK(
        event.input.inputSequence.get() == 1
    );

    /*
     * Vérification du tick client.
     */
    CHECK(
        event.input.clientTick.get() == CLIENT_TICK
    );

    /*
     * Vérification des actions.
     */
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

    /*
     * On ne doit pas recevoir d'action parasite.
     */
    CHECK(
        !rtype::protocol::hasAction(
            event.input.actions,
            rtype::protocol::InputAction::Left
        )
    );

    CHECK(
        !rtype::protocol::hasAction(
            event.input.actions,
            rtype::protocol::InputAction::Up
        )
    );

    std::cout
        << "[OK] Input sequence="
        << event.input.inputSequence.get()
        << " tick="
        << event.input.clientTick.get()
        << std::endl;
}

void testMultipleInputs(
    rtype::client::NetworkClient &client,
    rtype::server::Server &server,
    std::uint32_t connectionId
)
{
    std::cout
        << "[TEST] Multiple inputs..."
        << std::endl;

    /*
     * Premier input déjà envoyé :
     * inputSequence = 1
     *
     * On envoie maintenant les séquences 2, 3, 4.
     */
    client.sendInput(
        43,
        rtype::protocol::InputAction::Up
    );

    client.sendInput(
        44,
        rtype::protocol::InputAction::Down
    );

    client.sendInput(
        45,
        rtype::protocol::InputAction::Left
    );

    const rtype::protocol::InputAction expectedActions[] = {
        rtype::protocol::InputAction::Up,
        rtype::protocol::InputAction::Down,
        rtype::protocol::InputAction::Left
    };

    for (std::uint32_t i = 0; i < 3; ++i) {
        rtype::server::Server::InputEvent event;

        CHECK(
            waitForInput(
                server,
                event,
                2s
            )
        );

        CHECK(
            event.connectionId == connectionId
        );

        CHECK(
            event.input.inputSequence.get() == i + 2
        );

        CHECK(
            event.input.clientTick.get() == i + 43
        );

        CHECK(
            event.input.actions == expectedActions[i]
        );
    }

    std::cout
        << "[OK] Input sequences 2, 3, 4"
        << std::endl;
}

void testQueueEmpty(
    rtype::server::Server &server
)
{
    std::cout
        << "[TEST] Queue should be empty..."
        << std::endl;

    /*
     * On attend brièvement pour détecter d'éventuels
     * événements supplémentaires arrivant en retard.
     */
    std::this_thread::sleep_for(100ms);

    rtype::server::Server::InputEvent event;

    CHECK(!server.pollInput(event));

    std::cout
        << "[OK] Input queue empty"
        << std::endl;
}

void testShutdown(
    rtype::client::NetworkClient &client
)
{
    std::cout
        << "[TEST] Client shutdown..."
        << std::endl;

    client.stop();

    CHECK(!client.isRunning());
    CHECK(!client.isConnected());
    CHECK(client.getConnectionId() == 0);

    /*
     * Vérification de l'idempotence.
     * Un second stop() ne doit rien casser.
     */
    client.stop();

    std::cout
        << "[OK] Client shutdown"
        << std::endl;
}

} // namespace

int main()
{
    std::cout
        << "====================================\n"
        << " NetworkClient integration test\n"
        << "====================================\n";

    /*
     * Création du vrai serveur R-Type.
     */
    rtype::server::Server server(SERVER_PORT);

    server.start();

    /*
     * Création de notre nouveau NetworkClient.
     */
    rtype::client::NetworkClient client;

    std::uint32_t connectionId = 0;

    /*
     * Étape 1 : HELLO / WELCOME
     */
    testConnection(
        client,
        connectionId
    );

    /*
     * Étape 2 : premier Input
     */
    testInput(
        client,
        server,
        connectionId
    );

    /*
     * Étape 3 : plusieurs Inputs
     */
    testMultipleInputs(
        client,
        server,
        connectionId
    );

    /*
     * Étape 4 : queue vide
     */
    testQueueEmpty(server);

    /*
     * Étape 5 : arrêt
     */
    testShutdown(client);

    server.stop();

    std::cout
        << "\n====================================\n"
        << " NetworkClient integration test: OK\n"
        << "====================================\n";

    return EXIT_SUCCESS;
}
