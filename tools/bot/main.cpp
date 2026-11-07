#include "NetworkClient.hpp"
#include "Protocol/RoomPayload.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

using namespace rtype;
using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;

namespace {

struct Bot {
  client::NetworkClient net;
  std::uint32_t roomId = 0;
  bool connected = false;
  bool inRoom = false;
  bool ready = false;
  bool started = false;
  bool disconnected = false;
  std::uint64_t snapshots = 0, creates = 0, destroys = 0, others = 0;
  std::uint64_t snapshotEntities = 0;
  std::uint32_t tick = 0;
  protocol::InputAction held = protocol::InputAction::NoneAction;
  Clock::time_point nextChange{};
};

void pump(Bot &bot, bool creator, std::uint32_t &sharedRoom) {
  client::NetworkEvent event;
  while (bot.net.pollEvent(event)) {
    switch (event.type) {
    case client::NetworkEventType::Connected:
      bot.connected = true;
      break;
    case client::NetworkEventType::Disconnected:
    case client::NetworkEventType::Rejected:
      bot.disconnected = true;
      break;
    case client::NetworkEventType::Message: {
      using protocol::MessageType;
      if (event.messageType == MessageType::RoomCreated ||
          event.messageType == MessageType::RoomJoined) {
        if (event.payload.size() == 5 && event.payload[0] == 0) {
          bot.roomId = (std::uint32_t(event.payload[1]) << 24) |
                       (std::uint32_t(event.payload[2]) << 16) |
                       (std::uint32_t(event.payload[3]) << 8) |
                       std::uint32_t(event.payload[4]);
          bot.inRoom = true;
          if (creator)
            sharedRoom = bot.roomId;
        }
      } else if (event.messageType == MessageType::GameStarting) {
        bot.started = true;
      } else {
        ++bot.others;
      }
      break;
    }
    }
  }
  client::ReplicationEvent rep;
  while (bot.net.pollReplicationEvent(rep)) {
    if (const auto *snap = std::get_if<protocol::SnapshotPayload>(&rep.payload)) {
      ++bot.snapshots;
      bot.snapshotEntities += snap->entities.size();
    } else if (std::holds_alternative<protocol::CreateEntityPayload>(
                   rep.payload)) {
      ++bot.creates;
    } else {
      ++bot.destroys;
    }
  }
}

}

int main(int argc, char **argv) {
  const std::string host = argc > 1 ? argv[1] : "127.0.0.1";
  const std::string port = argc > 2 ? argv[2] : "4242";
  const int count = argc > 3 ? std::atoi(argv[3]) : 4;
  const int seconds = argc > 4 ? std::atoi(argv[4]) : 20;

  std::vector<std::unique_ptr<Bot>> bots;
  for (int i = 0; i < count; ++i) {
    bots.push_back(std::make_unique<Bot>());
    if (!bots.back()->net.connect(host, port)) {
      std::cerr << "bot " << i << ": connect failed\n";
      return 1;
    }
  }

  constexpr int ROOM_SIZE = 4;
  std::vector<std::uint32_t> sharedRooms((count + ROOM_SIZE - 1) / ROOM_SIZE, 0);
  std::mt19937 rng(42);
  auto step = [&]() {
    for (int i = 0; i < count; ++i)
      pump(*bots[i], i % ROOM_SIZE == 0, sharedRooms[i / ROOM_SIZE]);
  };
  auto waitFor = [&](auto pred, std::chrono::seconds limit) {
    const auto end = Clock::now() + limit;
    while (Clock::now() < end) {
      step();
      if (pred())
        return true;
      std::this_thread::sleep_for(5ms);
    }
    return false;
  };

  const auto helloEnd = Clock::now() + 10s;
  while (Clock::now() < helloEnd) {
    bool all = true;
    for (auto &b : bots) {
      if (!b->connected) {
        b->net.sendHello();
        all = false;
      }
    }
    step();
    if (all)
      break;
    std::this_thread::sleep_for(50ms);
  }
  for (int i = 0; i < count; ++i)
    if (!bots[i]->connected) {
      std::cerr << "FAIL: bot " << i << " never connected\n";
      return 1;
    }
  std::cout << count << " bots connected\n";

  for (int i = 0; i < count; i += ROOM_SIZE)
    bots[i]->net.sendCreateRoom("bots " + std::to_string(i / ROOM_SIZE));
  if (!waitFor(
          [&] {
            for (auto id : sharedRooms)
              if (id == 0)
                return false;
            return true;
          },
          10s)) {
    std::cerr << "FAIL: a room was not created\n";
    return 1;
  }
  for (int i = 0; i < count; ++i)
    if (i % ROOM_SIZE != 0)
      bots[i]->net.sendJoinRoom(sharedRooms[i / ROOM_SIZE]);
  if (!waitFor(
          [&] {
            for (auto &b : bots)
              if (!b->inRoom)
                return false;
            return true;
          },
          10s)) {
    std::cerr << "FAIL: not every bot joined its room\n";
    return 1;
  }
  std::cout << sharedRooms.size() << " room(s), all bots inside\n";

  for (auto &b : bots)
    b->net.sendSetReady(true);
  if (!waitFor(
          [&] {
            for (auto &b : bots)
              if (!b->started)
                return false;
            return true;
          },
          15s)) {
    std::cerr << "FAIL: game never started\n";
    return 1;
  }
  std::cout << "game started, playing " << seconds << " s\n";

  const auto playEnd = Clock::now() + std::chrono::seconds(seconds);
  auto next = Clock::now();
  while (Clock::now() < playEnd) {
    step();
    const auto now = Clock::now();
    for (auto &b : bots) {
      if (now >= b->nextChange) {
        std::uint8_t bits = 0;
        for (std::uint8_t flag : {0x01, 0x02, 0x04, 0x08})
          if (rng() % 3 == 0)
            bits |= flag;
        if (rng() % 2)
          bits |= 0x10;
        b->held = static_cast<protocol::InputAction>(bits);
        b->nextChange = now + std::chrono::milliseconds(150 + rng() % 600);
      }
      b->net.sendInput(++b->tick, b->held);
    }
    next += 16ms;
    std::this_thread::sleep_until(next);
  }
  step();

  int status = 0;
  std::uint64_t sent = 0, sentBytes = 0, recv = 0, recvBytes = 0;
  for (int i = 0; i < count; ++i) {
    auto &b = *bots[i];
    const auto m = b.net.getMetrics();
    std::cout << "bot " << i << ": snapshots=" << b.snapshots
              << " avgEntities="
              << (b.snapshots ? b.snapshotEntities / b.snapshots : 0)
              << " create=" << b.creates << " destroy=" << b.destroys
              << " other=" << b.others << " | sent " << m.packetsSent
              << " pkts/" << m.bytesSent << " B, recv " << m.packetsReceived
              << " pkts/" << m.bytesReceived << " B, loss "
              << m.packetLossPercentage << "%, rtt " << m.smoothRtt
              << " ms, retrans " << m.retransmissions
              << (b.disconnected ? " DISCONNECTED" : "") << "\n";
    sent += m.packetsSent;
    sentBytes += m.bytesSent;
    recv += m.packetsReceived;
    recvBytes += m.bytesReceived;
    if (b.snapshots == 0 || b.disconnected)
      status = 1;
  }
  std::cout << "TOTAL client->server " << sent << " pkts " << sentBytes
            << " B (" << (sent ? sentBytes / sent : 0)
            << " B/pkt) | server->client " << recv << " pkts " << recvBytes
            << " B (" << (recv ? recvBytes / recv : 0) << " B/pkt)\n";
  std::cout << (status == 0 ? "PASS" : "FAIL") << "\n";
  for (auto &b : bots)
    b->net.stop();
  return status;
}
