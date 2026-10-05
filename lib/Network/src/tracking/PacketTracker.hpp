#pragma once 

#include <cstdint>
enum class PacketReceiveRes {
    New,
    Dup,
    Out,
    TooOld
};

class PacketTracker {
  public:
    PacketTracker();
    std::uint32_t getNextSequence();
    PacketReceiveRes onPacketReceived(std::uint32_t sequence);
    std::uint32_t getAck() const;
    std::uint32_t getAckBits() const;
    static bool isPacketinAck(std::uint32_t sequence, std::uint32_t ack, std::uint32_t ackBits);

  private:
    std::uint32_t _nextSequence;
    bool _hasReceivedPacket;
    std::uint32_t _lastReceived;
    std::uint32_t _receivedBits;
};
