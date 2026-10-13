#include "NetworkClient.hpp"
#include "Server.hpp"

#include "Protocol/EntityPayload.hpp"
#include "Protocol/SnapshotPayload.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <variant>

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

constexpr std::uint16_t SERVER_PORT = 42434;

bool waitForConnected(
    rtype::client::NetworkClient &client,
    std::uint32_t &connectionId
)
{
    const auto deadline =
        std::chrono::steady_clock::now() + 2s;

    while (std::chrono::steady_clock::now() < deadline) {
        rtype::client::NetworkEvent event;

        while (client.pollEvent(event)) {
            if (
                event.type ==
                rtype::client::NetworkEventType::Connected
            ) {
                connectionId = event.connectionId;
                return connectionId != 0;
            }

            if (
                event.type ==
                rtype::client::NetworkEventType::Rejected
            ) {
                return false;
            }
        }

        std::this_thread::sleep_for(1ms);
    }

    return false;
}

bool waitForReplication(
    rtype::client::NetworkClient &client,
    rtype::client::ReplicationEvent &event
)
{
    const auto deadline =
        std::chrono::steady_clock::now() + 2s;

    while (std::chrono::steady_clock::now() < deadline) {
        if (client.pollReplicationEvent(event))
            return true;

        std::this_thread::sleep_for(1ms);
    }

    return false;
}

void testReplication()
{
    std::cout << "[TEST] Replication over UDP\n";

    rtype::server::Server server(SERVER_PORT);
    server.start();

    rtype::client::NetworkClient client;

    CHECK(
        client.connect(
            "127.0.0.1",
            std::to_string(SERVER_PORT)
        )
    );

    client.sendHello();

    std::uint32_t connectionId = 0;

    CHECK(waitForConnected(client, connectionId));
    CHECK(client.isConnected());

    std::cout << "[OK] Connected\n";

    rtype::protocol::CreateEntityPayload create;
    create.entityId = 42;
    create.blueprintId =
        rtype::protocol::BlueprintId::Player;
    create.x = 100.25f;
    create.y = -50.5f;

    CHECK(server.sendCreateEntity(connectionId, create));

    rtype::client::ReplicationEvent event;

    CHECK(waitForReplication(client, event));
    CHECK(event.connectionId == connectionId);

    const auto *created =
        std::get_if<rtype::protocol::CreateEntityPayload>(
            &event.payload
        );

    CHECK(created != nullptr);
    CHECK(created->entityId == 42);
    CHECK(
        created->blueprintId ==
        rtype::protocol::BlueprintId::Player
    );
    CHECK(created->x == 100.25f);
    CHECK(created->y == -50.5f);

    std::cout << "[OK] CreateEntity received\n";

    rtype::protocol::SnapshotPayload snapshot;
    snapshot.serverTick = 120;

    snapshot.entities = {
        {42, 150.25f, 200.5f},
        {43, -20.0f, 300.75f}
    };

    CHECK(server.sendSnapshot(connectionId, snapshot));

    CHECK(waitForReplication(client, event));
    CHECK(event.connectionId == connectionId);

    const auto *receivedSnapshot =
        std::get_if<rtype::protocol::SnapshotPayload>(
            &event.payload
        );

    CHECK(receivedSnapshot != nullptr);
    CHECK(receivedSnapshot->serverTick == 120);
    CHECK(receivedSnapshot->entities.size() == 2);

    CHECK(receivedSnapshot->entities[0].entityId == 42);
    CHECK(receivedSnapshot->entities[0].x == 150.25f);
    CHECK(receivedSnapshot->entities[0].y == 200.5f);

    CHECK(receivedSnapshot->entities[1].entityId == 43);
    CHECK(receivedSnapshot->entities[1].x == -20.0f);
    CHECK(receivedSnapshot->entities[1].y == 300.75f);

    std::cout << "[OK] Snapshot received\n";

    rtype::protocol::DestroyEntityPayload destroy;
    destroy.entityId = 42;

    CHECK(server.sendDestroyEntity(connectionId, destroy));

    CHECK(waitForReplication(client, event));
    CHECK(event.connectionId == connectionId);

    const auto *destroyed =
        std::get_if<rtype::protocol::DestroyEntityPayload>(
            &event.payload
        );

    CHECK(destroyed != nullptr);
    CHECK(destroyed->entityId == 42);

    std::cout << "[OK] DestroyEntity received\n";

    CHECK(!client.pollReplicationEvent(event));

    client.stop();
    server.stop();

    std::cout << "Replication integration test: OK\n";
}

}

int main()
{
    testReplication();
    return EXIT_SUCCESS;
}
