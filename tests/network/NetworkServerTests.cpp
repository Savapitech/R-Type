#include "NetworkTestUtils.hpp"
#include "Test.hpp"

#include <array>
#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "config/NetworkConfig.hpp"
#include "protocol/HandshakeHeader.hpp"
#include "protocol/MessageParser.hpp"
#include "protocol/PingMessage.hpp"
#include "protocol/PongMessage.hpp"
#include "protocol/SystemMessages.hpp"
#include "server/NetworkServer.hpp"

using rtype::test::datagram;
using rtype::test::frame;
using rtype::test::noise;
using rtype::test::peerHeader;
using Bytes = std::vector<std::uint8_t>;

namespace {

constexpr std::uint16_t FIRST_PORT = 47310;
constexpr auto WAIT = std::chrono::milliseconds(1000);

/// Starts a server on the first free port from FIRST_PORT.
struct RunningServer {
  std::unique_ptr<NetworkServer> server;
  std::uint16_t port = 0;

  RunningServer() {
    for (std::uint16_t candidate = FIRST_PORT; candidate < FIRST_PORT + 200;
         ++candidate) {
      try {
        server = std::make_unique<NetworkServer>(candidate);
        port = candidate;
        return;
      } catch (const std::exception &) {
        continue;
      }
    }
    rtype::test::fail(__FILE__, __LINE__, "no free UDP port for the server");
  }

  NetworkServer *operator->() { return server.get(); }
};

/// Bare UDP socket that plays the client side.
class Probe {
public:
  explicit Probe(std::uint16_t serverPort)
      : _socket(_io, asio::ip::udp::endpoint(asio::ip::udp::v4(), 0)),
        _server(asio::ip::make_address("127.0.0.1"), serverPort) {
    _socket.non_blocking(true);
  }

  void send(const Bytes &data) { _socket.send_to(asio::buffer(data), _server); }

  /// Next datagram from the server, or nothing after `timeout`.
  std::optional<Bytes> receive(std::chrono::milliseconds timeout = WAIT) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    std::array<std::uint8_t, 2048> buffer{};
    while (std::chrono::steady_clock::now() < deadline) {
      asio::ip::udp::endpoint from;
      asio::error_code error;
      const auto size =
          _socket.receive_from(asio::buffer(buffer), from, 0, error);
      if (!error)
        return Bytes(buffer.begin(), buffer.begin() + size);
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return std::nullopt;
  }

  /// Next connected packet that carries messages, ack-only ones are skipped.
  std::optional<std::vector<ParsedMessage>>
  receiveMessages(std::chrono::milliseconds timeout = WAIT) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
      auto data = receive(std::chrono::duration_cast<std::chrono::milliseconds>(
          deadline - std::chrono::steady_clock::now()));
      if (!data || data->size() <= PacketHeader::HEADER_SIZE)
        continue;
      PacketReader reader(*data);
      PacketHeader header;
      std::vector<ParsedMessage> messages;
      if (header.deserialize(reader) && MessageParser::parse(reader, messages))
        return messages;
    }
    return std::nullopt;
  }

  /// Sends Hello and returns the connection id of the Welcome.
  std::optional<std::uint32_t> connect() {
    send(hello());
    const auto answer = receive();
    if (!answer)
      return std::nullopt;
    PacketReader reader(*answer);
    HandshakeHeader header;
    std::uint32_t id = 0;
    if (!header.deserialize(reader) || header.type != HandshakeType::Welcome ||
        !reader.readUInt32(id))
      return std::nullopt;
    return id;
  }

  /// Sends a connected packet holding `body`.
  void sendPacket(std::uint32_t connectionId, std::uint32_t sequence,
                  const Bytes &body,
                  PacketPriority priority = PacketPriority::Unreliable,
                  std::uint16_t reliableId = 0) {
    auto header = peerHeader(sequence, 0, 0, priority, reliableId);
    header.connectionId = connectionId;
    send(datagram(header, body));
  }

  static Bytes hello(std::uint8_t version = VERSION,
                     HandshakeType type = HandshakeType::Hello) {
    PacketWriter writer;
    HandshakeHeader header;
    header.version = version;
    header.type = type;
    header.serialize(writer);
    return writer.getData();
  }

private:
  asio::io_context _io;
  asio::ip::udp::socket _socket;
  asio::ip::udp::endpoint _server;
};

/// Waits until `condition` holds or the delay expires.
template <typename Condition> bool eventually(Condition condition) {
  const auto deadline = std::chrono::steady_clock::now() + WAIT;
  while (std::chrono::steady_clock::now() < deadline) {
    if (condition())
      return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  return condition();
}

/// Messages received by the game side of the server.
struct Inbox {
  std::mutex mutex;
  std::vector<std::pair<MessageId, Bytes>> messages;

  void attach(NetworkServer &server) {
    server.setOnMessage(
        [this](std::uint32_t, MessageId type, const Bytes &payload) {
          std::lock_guard<std::mutex> lock(mutex);
          messages.emplace_back(type, payload);
        });
  }

  std::size_t size() {
    std::lock_guard<std::mutex> lock(mutex);
    return messages.size();
  }
};

} // namespace

TEST(NetworkServer, HelloIsAnsweredByWelcome) {
  RunningServer server;
  server->start();
  Probe probe(server.port);

  probe.send(Probe::hello());
  const auto answer = probe.receive();
  CHECK(answer.has_value());
  CHECK_EQ(answer->size(), HandshakeHeader::HEADER_SIZE + 4);
  PacketReader reader(*answer);
  HandshakeHeader header;
  std::uint32_t id = 0;
  CHECK(header.deserialize(reader));
  CHECK_EQ(header.type, HandshakeType::Welcome);
  CHECK_EQ(header.version, VERSION);
  CHECK(reader.readUInt32(id));
  CHECK_EQ(id, 1u);
}

TEST(NetworkServer, ClientsGetDistinctIds) {
  RunningServer server;
  server->start();
  Probe first(server.port);
  Probe second(server.port);

  const auto firstId = first.connect();
  const auto secondId = second.connect();
  CHECK(firstId.has_value());
  CHECK(secondId.has_value());
  CHECK_NE(*firstId, *secondId);
}

TEST(NetworkServer, SecondHelloKeepsTheSameId) {
  RunningServer server;
  std::atomic<int> connected{0};
  server->setOnClientConnected([&](std::uint32_t) { ++connected; });
  server->start();
  Probe probe(server.port);

  const auto first = probe.connect();
  const auto second = probe.connect();
  CHECK(first.has_value());
  CHECK(second.has_value());
  CHECK_EQ(*first, *second);
  CHECK_EQ(connected.load(), 1);
}

TEST(NetworkServer, ConnectedCallbackGetsTheId) {
  RunningServer server;
  std::atomic<std::uint32_t> connectedId{0};
  server->setOnClientConnected([&](std::uint32_t id) { connectedId = id; });
  server->start();
  Probe probe(server.port);

  const auto id = probe.connect();
  CHECK(id.has_value());
  CHECK(eventually([&] { return connectedId.load() == *id; }));
}

TEST(NetworkServer, WrongVersionIsIgnored) {
  RunningServer server;
  server->start();
  Probe probe(server.port);

  probe.send(Probe::hello(static_cast<std::uint8_t>(VERSION + 1)));
  CHECK_FALSE(probe.receive(std::chrono::milliseconds(200)).has_value());
}

TEST(NetworkServer, BadMagicIsIgnored) {
  RunningServer server;
  server->start();
  Probe probe(server.port);

  auto hello = Probe::hello();
  hello[0] = 0;
  probe.send(hello);
  CHECK_FALSE(probe.receive(std::chrono::milliseconds(200)).has_value());
}

TEST(NetworkServer, HelloWithTrailingBytesIsIgnored) {
  RunningServer server;
  server->start();
  Probe probe(server.port);

  auto hello = Probe::hello();
  hello.push_back(0);
  probe.send(hello);
  CHECK_FALSE(probe.receive(std::chrono::milliseconds(200)).has_value());
}

TEST(NetworkServer, WelcomeSentByAClientIsIgnored) {
  RunningServer server;
  server->start();
  Probe probe(server.port);

  probe.send(Probe::hello(VERSION, HandshakeType::Welcome));
  CHECK_FALSE(probe.receive(std::chrono::milliseconds(200)).has_value());
}

TEST(NetworkServer, PingIsAnsweredByPong) {
  RunningServer server;
  server->start();
  Probe probe(server.port);
  const auto id = probe.connect();
  CHECK(id.has_value());

  PacketWriter writer;
  PingMessage ping;
  ping.timestamp = 0x01020304;
  ping.serialize(writer);
  probe.sendPacket(*id, 1, frame(systemMessage::Ping, writer.getData()));

  const auto messages = probe.receiveMessages();
  CHECK(messages.has_value());
  CHECK_EQ(messages->size(), 1u);
  CHECK_EQ(messages->front().type, systemMessage::Pong);
  PacketReader reader(messages->front().payload);
  PongMessage pong;
  CHECK(pong.deserialize(reader));
  CHECK_EQ(pong.timestamp, 0x01020304u);
}

TEST(NetworkServer, GameMessagesReachTheCallback) {
  RunningServer server;
  Inbox inbox;
  inbox.attach(*server.server);
  server->start();
  Probe probe(server.port);
  const auto id = probe.connect();
  CHECK(id.has_value());

  const auto payload = noise(24, 5);
  probe.sendPacket(*id, 1, frame(0x0042, payload));
  CHECK(eventually([&] { return inbox.size() == 1; }));
  std::lock_guard<std::mutex> lock(inbox.mutex);
  CHECK_EQ(inbox.messages[0].first, 0x0042);
  CHECK(inbox.messages[0].second == payload);
}

TEST(NetworkServer, SeveralMessagesOfAPacketAreDelivered) {
  RunningServer server;
  Inbox inbox;
  inbox.attach(*server.server);
  server->start();
  Probe probe(server.port);
  const auto id = probe.connect();
  CHECK(id.has_value());

  auto body = frame(0x0040, noise(8, 1));
  const auto second = frame(0x0041, noise(8, 2));
  body.insert(body.end(), second.begin(), second.end());
  probe.sendPacket(*id, 1, body);
  CHECK(eventually([&] { return inbox.size() == 2; }));
}

TEST(NetworkServer, DuplicatedPacketIsDeliveredOnce) {
  RunningServer server;
  Inbox inbox;
  inbox.attach(*server.server);
  server->start();
  Probe probe(server.port);
  const auto id = probe.connect();
  CHECK(id.has_value());

  const auto body = frame(0x0042, noise(8));
  probe.sendPacket(*id, 1, body);
  probe.sendPacket(*id, 1, body);
  probe.sendPacket(*id, 2, frame(0x0043, noise(8, 2)));
  CHECK(eventually([&] { return inbox.size() == 2; }));
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  CHECK_EQ(inbox.size(), 2u);
}

TEST(NetworkServer, ResentReliablePacketIsDeliveredOnce) {
  RunningServer server;
  Inbox inbox;
  inbox.attach(*server.server);
  server->start();
  Probe probe(server.port);
  const auto id = probe.connect();
  CHECK(id.has_value());

  const auto body = frame(0x0042, noise(8));
  probe.sendPacket(*id, 1, body, PacketPriority::Reliable, 1);
  probe.sendPacket(*id, 2, body, PacketPriority::Reliable, 1);
  probe.sendPacket(*id, 3, frame(0x0043, noise(8, 2)));
  CHECK(eventually([&] { return inbox.size() == 2; }));
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  CHECK_EQ(inbox.size(), 2u);
}

TEST(NetworkServer, ReliablePacketIsAcked) {
  RunningServer server;
  server->start();
  Probe probe(server.port);
  const auto id = probe.connect();
  CHECK(id.has_value());

  probe.sendPacket(*id, 9, frame(0x0042, noise(8)), PacketPriority::Reliable,
                   1);
  const auto answer = probe.receive();
  CHECK(answer.has_value());
  const auto header = rtype::test::headerOf(*answer);
  CHECK_EQ(header.connectionId, *id);
  CHECK_EQ(header.ack, 9u);
}

TEST(NetworkServer, UnknownConnectionIdIsIgnored) {
  RunningServer server;
  Inbox inbox;
  inbox.attach(*server.server);
  server->start();
  Probe probe(server.port);
  const auto id = probe.connect();
  CHECK(id.has_value());

  probe.sendPacket(*id + 100, 1, frame(0x0042, noise(8)));
  probe.sendPacket(*id, 2, frame(0x0043, noise(8)));
  CHECK(eventually([&] { return inbox.size() == 1; }));
  std::lock_guard<std::mutex> lock(inbox.mutex);
  CHECK_EQ(inbox.messages[0].first, 0x0043);
}

TEST(NetworkServer, PacketFromAnotherEndpointIsIgnored) {
  RunningServer server;
  Inbox inbox;
  inbox.attach(*server.server);
  server->start();
  Probe owner(server.port);
  Probe intruder(server.port);
  const auto id = owner.connect();
  CHECK(id.has_value());
  CHECK(intruder.connect().has_value());

  intruder.sendPacket(*id, 1, frame(0x0042, noise(8)));
  owner.sendPacket(*id, 1, frame(0x0043, noise(8)));
  CHECK(eventually([&] { return inbox.size() == 1; }));
  std::lock_guard<std::mutex> lock(inbox.mutex);
  CHECK_EQ(inbox.messages[0].first, 0x0043);
}

TEST(NetworkServer, InvalidFramingIsDropped) {
  RunningServer server;
  Inbox inbox;
  inbox.attach(*server.server);
  server->start();
  Probe probe(server.port);
  const auto id = probe.connect();
  CHECK(id.has_value());

  probe.sendPacket(*id, 1, {0x00, 0x42, 0x00, 0x10, 0x01});
  probe.sendPacket(*id, 2, frame(0x0043, noise(8)));
  CHECK(eventually([&] { return inbox.size() == 1; }));
  std::lock_guard<std::mutex> lock(inbox.mutex);
  CHECK_EQ(inbox.messages[0].first, 0x0043);
}

TEST(NetworkServer, DisconnectMessageClosesTheConnection) {
  RunningServer server;
  std::atomic<std::uint32_t> disconnected{0};
  server->setOnClientDisconnected([&](std::uint32_t id) { disconnected = id; });
  server->start();
  Probe probe(server.port);
  const auto id = probe.connect();
  CHECK(id.has_value());

  probe.sendPacket(*id, 1, frame(systemMessage::Disconnect, {}));
  CHECK(eventually([&] { return disconnected.load() == *id; }));
}

TEST(NetworkServer, ReconnectAfterDisconnectGetsANewId) {
  RunningServer server;
  server->start();
  Probe probe(server.port);
  const auto first = probe.connect();
  CHECK(first.has_value());

  probe.sendPacket(*first, 1, frame(systemMessage::Disconnect, {}));
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  const auto second = probe.connect();
  CHECK(second.has_value());
  CHECK_NE(*first, *second);
}

TEST(NetworkServer, SendReachesTheClient) {
  RunningServer server;
  std::atomic<std::uint32_t> connectedId{0};
  server->setOnClientConnected([&](std::uint32_t id) { connectedId = id; });
  server->start();
  Probe probe(server.port);
  const auto id = probe.connect();
  CHECK(id.has_value());
  CHECK(eventually([&] { return connectedId.load() == *id; }));

  const auto payload = noise(40, 11);
  server->send(*id, frame(0x0090, payload), PacketPriority::Reliable);
  const auto messages = probe.receiveMessages();
  CHECK(messages.has_value());
  CHECK_EQ(messages->size(), 1u);
  CHECK_EQ(messages->front().type, 0x0090);
  CHECK(messages->front().payload == payload);
}

TEST(NetworkServer, SendToAnUnknownClientIsIgnored) {
  RunningServer server;
  server->start();

  CHECK_NOTHROW(server->send(999, frame(0x0090, noise(8))));
  CHECK_NOTHROW(server->flushAll());
}

TEST(NetworkServer, SendBeforeStartIsIgnored) {
  RunningServer server;

  CHECK_NOTHROW(server->send(1, frame(0x0090, noise(8))));
  CHECK_NOTHROW(server->flushAll());
}

TEST(NetworkServer, StopIsIdempotent) {
  RunningServer server;
  server->start();

  CHECK_NOTHROW(server->stop());
  CHECK_NOTHROW(server->stop());
}

TEST(NetworkServer, RejectsClientsPastTheLimit) {
  RunningServer server;
  server->start();
  std::vector<std::unique_ptr<Probe>> probes;
  for (std::size_t i = 0; i < MAX_CONNECTIONS; ++i) {
    probes.push_back(std::make_unique<Probe>(server.port));
    CHECK(probes.back()->connect().has_value());
  }

  Probe late(server.port);
  late.send(Probe::hello());
  const auto answer = late.receive();
  CHECK(answer.has_value());
  PacketReader reader(*answer);
  HandshakeHeader header;
  CHECK(header.deserialize(reader));
  CHECK_EQ(header.type, HandshakeType::Rejected);
  CHECK(reader.isEnd());
}

TEST(NetworkServer, CountsReceivedPacketsPerClient) {
  RunningServer server;
  Inbox inbox;
  inbox.attach(*server.server);
  server->start();
  Probe probe(server.port);
  const auto id = probe.connect();
  CHECK(id.has_value());

  for (std::uint32_t sequence = 1; sequence <= 3; ++sequence)
    probe.sendPacket(*id, sequence, frame(0x0042, noise(8, sequence)));
  CHECK(eventually([&] { return inbox.size() == 3; }));
  CHECK(eventually([&] {
    const auto all = server->getAllClientMetrics();
    const auto it = all.find(*id);
    return it != all.end() && it->second.packetsReceived == 3;
  }));
}
