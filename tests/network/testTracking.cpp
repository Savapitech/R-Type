#include <cassert>
#include <cstdint>
#include <iostream>

#include "tracking/PacketTracker.hpp"

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

    std::cout << "PacketTracker tests: OK\n";

    return 0;
}
