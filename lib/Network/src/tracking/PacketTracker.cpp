#include "PacketTracker.hpp"
#include <cstdint>

PacketTracker::PacketTracker() :
  _nextSequence(1), _hasReceivedPacket(false), _lastReceived(0), _receivedBits(0) 
{}

std::uint32_t PacketTracker::getNextSequence() {
  return _nextSequence++;
}

PacketReceiveRes PacketTracker::onPacketReceived(std::uint32_t sequence) {
  if (!_hasReceivedPacket) {
    _hasReceivedPacket = true;
    _lastReceived = sequence;
    _receivedBits = 0;
    return PacketReceiveRes::New;
  }

  if (sequence == _lastReceived)
    return PacketReceiveRes::Dup;

  if (sequence > _lastReceived) {
    const std::uint32_t diff = sequence -_lastReceived;
    if (diff > 32) {
      _receivedBits = 0;
    } else if (diff == 32) {
      _receivedBits = 1u << 31;
    } else {
      _receivedBits <<= diff;
      _receivedBits |= 1u << (diff - 1);
    }
    _lastReceived = sequence;
    return PacketReceiveRes::New;
  }

  const std::uint32_t diff = _lastReceived - sequence;
  if (diff > 32)
    return PacketReceiveRes::TooOld;

  const std::uint32_t mask = 1u << (diff - 1);
  if ((_receivedBits & mask) != 0)
    return PacketReceiveRes::Dup;
  _receivedBits |= mask;
  return PacketReceiveRes::Out;
}

bool PacketTracker::isPacketinAck(std::uint32_t sequence, std::uint32_t ack, std::uint32_t ackBits) {
  if (sequence == ack)
    return true;
  
  if (sequence > ack)
    return false;

  const std::uint32_t diff = ack - sequence;
  if (diff == 0 || diff > 32)
    return false;

  return (ackBits & (1u << (diff - 1))) != 0;
}

std::uint32_t PacketTracker::getAck() const {
  return _hasReceivedPacket ? _lastReceived : 0;
}

std::uint32_t PacketTracker::getAckBits() const {
  return _hasReceivedPacket ? _receivedBits : 0;
}


