#include <cassert>
#include <chrono>
#include <random>
#include <set>
#include <vector>
#include <cstdint>
#include <iostream>

#include "config/NetworkConfig.hpp"
#include "protocol/PacketHeader.hpp"
#include "protocol/PacketReader.hpp"
#include "tracking/PacketTracker.hpp"

using Clock = std::chrono::steady_clock;
using std::chrono::milliseconds;

static PacketHeader headerOf(const std::vector<std::uint8_t> &datagram,
                             std::vector<std::uint8_t> *body = nullptr)
{
    PacketReader reader(datagram);
    PacketHeader header;
    bool ok = header.deserialize(reader);
    assert(ok);
    if (body)
        body->assign(datagram.begin() + PacketHeader::HEADER_SIZE,
                     datagram.end());
    return header;
}

static bool deliver(PacketTracker &to, const std::vector<std::uint8_t> &d,
                    Clock::time_point now, std::vector<std::uint8_t> *body)
{
    PacketHeader header = headerOf(d, body);
    return to.onPacketReceived(header, now);
}

static void testPriorityOrder()
{
    PacketTracker t;
    auto now = Clock::now();
    t.enqueue({2}, PacketPriority::Unreliable);
    t.enqueue({1}, PacketPriority::Reliable);
    t.enqueue({0}, PacketPriority::Critical);
    t.enqueue({0}, PacketPriority::Critical);

    auto out = t.collectOutgoing(now, 7);
    assert(out.size() == 4);
    assert(headerOf(out[0]).priority == PacketPriority::Critical);
    assert(headerOf(out[1]).priority == PacketPriority::Critical);
    assert(headerOf(out[2]).priority == PacketPriority::Reliable);
    assert(headerOf(out[3]).priority == PacketPriority::Unreliable);
    assert(headerOf(out[3]).reliableId == 0);
    assert(headerOf(out[0]).connectionId == 7);
}

static void testUnreliableNeverResent()
{
    PacketTracker t;
    auto now = Clock::now();
    t.enqueue({1}, PacketPriority::Unreliable);
    assert(t.collectOutgoing(now, 1).size() == 1);
    for (int i = 1; i < 50; ++i)
        assert(t.collectOutgoing(now + milliseconds(100 * i), 1).empty());
}

static void testReliableResendLimit()
{
    PacketTracker t;
    auto now = Clock::now();
    t.enqueue({1}, PacketPriority::Reliable);
    assert(t.collectOutgoing(now, 1).size() == 1);

    std::set<std::uint32_t> sequences;
    int resends = 0;
    for (int i = 1; i < 100; ++i) {
        auto out = t.collectOutgoing(now + milliseconds(100 * i), 1);
        for (auto &d : out) {
            assert(headerOf(d).reliableId == 1);
            sequences.insert(headerOf(d).sequence);
            ++resends;
        }
    }
    assert(resends == 20);
    assert(sequences.size() == 20);
    assert(t.getDroppedCount() == 1);
    assert(!t.isTimedOut());
}

static void testCriticalTimeout()
{
    PacketTracker t;
    auto now = Clock::now();
    for (int i = 0; i < 3; ++i)
        t.enqueue({1}, PacketPriority::Critical);
    t.collectOutgoing(now, 1);
    assert(!t.isTimedOut());

    for (int i = 1; i < 30 && !t.isTimedOut(); ++i)
        t.collectOutgoing(now + milliseconds(100 * i), 1);
    assert(t.isTimedOut());
    assert(t.getCriticalLossCount() == 3);
    assert(!t.collectOutgoing(now + milliseconds(100 * 40), 1).empty());
}

static void testCriticalLossDecay()
{
    PacketTracker t;
    auto now = Clock::now();
    t.enqueue({1}, PacketPriority::Critical);
    t.collectOutgoing(now, 1);

    auto at = now;
    while (t.getCriticalLossCount() == 0) {
        at += milliseconds(100);
        t.collectOutgoing(at, 1);
    }
    assert(t.getCriticalLossCount() == 1);
    t.collectOutgoing(at + CRITICAL_LOSS_DECAY - milliseconds(1), 1);
    assert(t.getCriticalLossCount() == 1);
    t.collectOutgoing(at + CRITICAL_LOSS_DECAY, 1);
    assert(t.getCriticalLossCount() == 0);
}

static void testAckStopsResend()
{
    PacketTracker a, b;
    auto now = Clock::now();
    a.enqueue({9}, PacketPriority::Critical);
    auto out = a.collectOutgoing(now, 1);
    assert(out.size() == 1);

    std::vector<std::uint8_t> body;
    assert(deliver(b, out[0], now, &body));
    assert(body == std::vector<std::uint8_t>{9});

    assert(b.collectOutgoing(now + milliseconds(10), 1).empty());
    auto ack = b.collectOutgoing(now + milliseconds(60), 1);
    assert(ack.size() == 1);
    assert(headerOf(ack[0], &body).priority == PacketPriority::Unreliable);
    assert(body.empty());
    assert(b.collectOutgoing(now + milliseconds(70), 1).empty());

    assert(deliver(a, ack[0], now + milliseconds(60), &body));
    assert(a.collectOutgoing(now + milliseconds(500), 1).empty());
    assert(a.getResendCount() == 0);
    assert(a.collectOutgoing(now + milliseconds(900), 1).empty());
}

static void testResendDeduplicated()
{
    PacketTracker a, b;
    auto now = Clock::now();
    a.enqueue({5}, PacketPriority::Critical);
    auto first = a.collectOutgoing(now, 1);
    auto second = a.collectOutgoing(now + milliseconds(110), 1);
    assert(first.size() == 1 && second.size() == 1);
    assert(headerOf(first[0]).sequence != headerOf(second[0]).sequence);

    std::vector<std::uint8_t> body;
    assert(deliver(b, second[0], now, &body));
    assert(!deliver(b, first[0], now, &body));
    assert(!deliver(b, second[0], now, &body));
}

static void testLossyLink(double loss, double dupe)
{
    PacketTracker a, b;
    std::mt19937 rng(1234);
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    struct InFlight { std::vector<std::uint8_t> d; Clock::time_point at; bool toB; };
    std::vector<InFlight> wire;
    auto now = Clock::now();

    std::multiset<int> received;
    constexpr int COUNT = 200;
    int sent = 0;

    auto transmit = [&](std::vector<std::uint8_t> d, bool toB) {
        if (dist(rng) < loss)
            return;
        auto lat = milliseconds(5 + static_cast<int>(dist(rng) * 55));
        wire.push_back({d, now + lat, toB});
        if (dist(rng) < dupe)
            wire.push_back({d, now + lat + milliseconds(20), toB});
    };

    for (int step = 0; step < 20000; ++step) {
        now += milliseconds(5);
        if (sent < COUNT && step % 4 == 0) {
            a.enqueue({static_cast<std::uint8_t>(sent)}, PacketPriority::Critical);
            ++sent;
        }
        for (auto &d : a.collectOutgoing(now, 1)) transmit(d, true);
        for (auto &d : b.collectOutgoing(now, 1)) transmit(d, false);

        for (auto it = wire.begin(); it != wire.end();) {
            if (it->at > now) { ++it; continue; }
            std::vector<std::uint8_t> body;
            PacketTracker &dst = it->toB ? b : a;
            if (deliver(dst, it->d, now, &body) && it->toB && !body.empty())
                received.insert(body[0]);
            it = wire.erase(it);
        }
    }
    assert(sent == COUNT);
    for (int i = 0; i < COUNT; ++i)
        assert(received.count(i) == 1);
}


int main()
{
    std::cout << "PacketTracker tests...\n";

    {
        PacketTracker tracker;

        assert(tracker.getNextSequence() == 1);
        assert(tracker.getNextSequence() == 2);
        assert(tracker.getNextSequence() == 3);
    }

    {
        PacketTracker tracker;

        assert(tracker.getAck() == 0);
        assert(tracker.getAckBits() == 0);
    }

    {
        PacketTracker tracker;

        assert(
            tracker.onPacketReceived(10)
            == PacketReceiveRes::New
        );

        assert(tracker.getAck() == 10);
        assert(tracker.getAckBits() == 0);
    }

    {
        PacketTracker tracker;

        tracker.onPacketReceived(10);
        tracker.onPacketReceived(11);

        assert(tracker.getAck() == 11);
        assert(tracker.getAckBits() == 0b1);
    }

    {
        PacketTracker tracker;

        tracker.onPacketReceived(10);
        tracker.onPacketReceived(11);
        tracker.onPacketReceived(13);

        assert(tracker.getAck() == 13);
        assert(tracker.getAckBits() == 0b110);

        assert(
            tracker.onPacketReceived(12)
            == PacketReceiveRes::Out
        );

        assert(tracker.getAck() == 13);
        assert(tracker.getAckBits() == 0b111);

        assert(
            tracker.onPacketReceived(12)
            == PacketReceiveRes::Dup
        );
    }

    {
        PacketTracker tracker;

        tracker.onPacketReceived(100);

        assert(
            tracker.onPacketReceived(50)
            == PacketReceiveRes::TooOld
        );
    }

    {
        PacketTracker tracker;

        tracker.onPacketReceived(10);
        tracker.onPacketReceived(100);

        assert(tracker.getAck() == 100);
        assert(tracker.getAckBits() == 0);
    }

    {
        std::uint32_t ack = 13;
        std::uint32_t ackBits = 0b110;

        assert(
            PacketTracker::isPacketinAck(
                13,
                ack,
                ackBits
            )
        );

        assert(
            !PacketTracker::isPacketinAck(
                12,
                ack,
                ackBits
            )
        );

        assert(
            PacketTracker::isPacketinAck(
                11,
                ack,
                ackBits
            )
        );

        assert(
            PacketTracker::isPacketinAck(
                10,
                ack,
                ackBits
            )
        );
    }

    testPriorityOrder();
    testUnreliableNeverResent();
    testReliableResendLimit();
    testCriticalTimeout();
    testCriticalLossDecay();
    testAckStopsResend();
    testResendDeduplicated();
    testLossyLink(0.02, 0.02);
    testLossyLink(0.30, 0.10);

    std::cout << "PacketTracker tests: OK\n";

    return 0;
}
