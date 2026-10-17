#include "PacketTracker.hpp"
#include "../config/NetworkConfig.hpp"
#include "../protocol/PacketWriter.hpp"
#include <algorithm>
#include <cstdint>

PacketTracker::PacketTracker()
    : _nextSequence(1), _hasReceivedPacket(false), _lastReceived(0),
      _receivedBits(0) {}

std::uint32_t PacketTracker::getNextSequence() { return _nextSequence++; }

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
    const std::uint32_t diff = sequence - _lastReceived;
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

bool PacketTracker::isPacketinAck(std::uint32_t sequence, std::uint32_t ack,
                                  std::uint32_t ackBits) {
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

bool PacketTracker::enqueue(std::vector<std::uint8_t> payload,
                            PacketPriority priority) {
  if (payload.size() > MAX_MESSAGE_SIZE)
    return false;

  _queues[static_cast<std::size_t>(priority)].push_back(std::move(payload));
  return true;
}

std::vector<std::uint8_t>
PacketTracker::buildPacket(std::uint32_t connectionId, PacketPriority priority,
                           std::uint16_t reliableId,
                           const std::vector<std::uint8_t> &body,
                           std::uint32_t &sequence) {
  PacketHeader header;
  header.connectionId = connectionId;
  header.sequence = getNextSequence();
  header.ack = getAck();
  header.ackBits = getAckBits();
  header.priority = priority;
  header.reliableId = reliableId;
  sequence = header.sequence;

  PacketWriter writer;
  if (!header.serialize(writer) || !writer.writeBytes(body.data(), body.size()))
    return {};
  return writer.getData();
}

std::chrono::milliseconds PacketTracker::resendDelay() const {
  const auto rtt =
      std::chrono::duration_cast<std::chrono::milliseconds>(_roundTrip * 3 / 2);
  return std::max(RESEND_TIMEOUT, rtt);
}

std::vector<std::vector<std::uint8_t>>
PacketTracker::collectOutgoing(TimePoint now, std::uint32_t connectionId) {
  std::vector<std::vector<std::uint8_t>> out;

  if (CRITICAL_LOSS_DECAY.count() > 0 && _criticalLosses > 0 && !isTimedOut() &&
      now - _lastLossChange >= CRITICAL_LOSS_DECAY) {
    --_criticalLosses;
    _lastLossChange = now;
  }

  for (std::size_t level = 0; level < 3; ++level) {
    const auto priority = static_cast<PacketPriority>(level);

    if (priority != PacketPriority::Unreliable) {
      std::vector<Pending *> due;
      for (auto &[id, pending] : _pending) {
        if (pending.priority == priority &&
            now - pending.lastSend >= resendDelay())
          due.push_back(&pending);
      }
      std::sort(due.begin(), due.end(), [](const Pending *a, const Pending *b) {
        return a->order < b->order;
      });

      for (Pending *pending : due) {
        if (priority == PacketPriority::Reliable &&
            pending->resends >= RELIABLE_MAX_RESENDS) {
          ++_droppedCount;
          _pending.erase(pending->reliableId);
          continue;
        }
        if (priority == PacketPriority::Critical && !pending->lossCounted &&
            pending->resends >= CRITICAL_RESENDS_BEFORE_LOSS) {
          pending->lossCounted = true;
          ++_criticalLosses;
          _lastLossChange = now;
        }

        std::uint32_t sequence = 0;
        auto packet = buildPacket(connectionId, priority, pending->reliableId,
                                  pending->payload, sequence);
        if (packet.empty())
          continue;

        ++pending->resends;
        ++_resendCount;
        pending->lastSend = now;
        pending->sequences.push_back(sequence);
        if (pending->sequences.size() > MAX_TRACKED_SEQUENCES)
          pending->sequences.erase(pending->sequences.begin());
        out.push_back(std::move(packet));
      }
    }

    auto &queue = _queues[level];
    while (!queue.empty()) {
      std::vector<std::uint8_t> body = std::move(queue.front());
      queue.pop_front();

      std::uint16_t reliableId = 0;
      if (priority != PacketPriority::Unreliable) {
        reliableId = _nextReliableId++;
        if (_nextReliableId == 0)
          _nextReliableId = 1;
      }

      std::uint32_t sequence = 0;
      auto packet =
          buildPacket(connectionId, priority, reliableId, body, sequence);
      if (packet.empty())
        continue;

      if (priority != PacketPriority::Unreliable) {
        Pending pending;
        pending.payload = std::move(body);
        pending.priority = priority;
        pending.reliableId = reliableId;
        pending.order = _nextOrder++;
        pending.lastSend = now;
        pending.sequences.push_back(sequence);
        _pending[reliableId] = std::move(pending);
      }
      out.push_back(std::move(packet));
    }
  }

  if (!out.empty()) {
    _ackDue = false;
  } else if (_ackDue && now - _ackDueSince >= ACK_ONLY_DELAY) {
    std::uint32_t sequence = 0;
    auto packet =
        buildPacket(connectionId, PacketPriority::Unreliable, 0, {}, sequence);
    if (!packet.empty())
      out.push_back(std::move(packet));
    _ackDue = false;
  }
  return out;
}

void PacketTracker::acknowledge(std::uint32_t ack, std::uint32_t ackBits,
                                TimePoint now) {
  if (ack == 0)
    return;

  for (auto it = _pending.begin(); it != _pending.end();) {
    const bool acked =
        std::any_of(it->second.sequences.begin(), it->second.sequences.end(),
                    [&](std::uint32_t sequence) {
                      return isPacketinAck(sequence, ack, ackBits);
                    });
    if (!acked) {
      ++it;
      continue;
    }

    if (it->second.resends == 0) {
      const auto sample = std::chrono::duration_cast<std::chrono::microseconds>(
          now - it->second.lastSend);
      _roundTrip =
          _roundTrip.count() == 0 ? sample : (_roundTrip * 7 + sample) / 8;
    }
    it = _pending.erase(it);
  }
}

bool PacketTracker::markReliableReceived(std::uint16_t reliableId) {
  if (!_hasReliableReceived) {
    _hasReliableReceived = true;
    _highestReliable = reliableId;
    _reliableSeen.reset();
    _reliableSeen.set(0);
    return true;
  }

  const auto diff = static_cast<std::int16_t>(reliableId - _highestReliable);
  if (diff > 0) {
    if (static_cast<std::size_t>(diff) >= RELIABLE_WINDOW)
      _reliableSeen.reset();
    else
      _reliableSeen <<= static_cast<std::size_t>(diff);
    _reliableSeen.set(0);
    _highestReliable = reliableId;
    return true;
  }

  const std::size_t back = static_cast<std::size_t>(-diff);
  if (back >= RELIABLE_WINDOW || _reliableSeen.test(back))
    return false;
  _reliableSeen.set(back);
  return true;
}

bool PacketTracker::onPacketReceived(const PacketHeader &header,
                                     TimePoint now) {
  acknowledge(header.ack, header.ackBits, now);

  const PacketReceiveRes res = onPacketReceived(header.sequence);
  if (res == PacketReceiveRes::Dup || res == PacketReceiveRes::TooOld)
    return false;

  if (header.priority == PacketPriority::Unreliable)
    return true;

  if (!_ackDue) {
    _ackDue = true;
    _ackDueSince = now;
  }
  return markReliableReceived(header.reliableId);
}

bool PacketTracker::isTimedOut() const {
  return _criticalLosses >= MAX_CRITICAL_LOSSES;
}

std::uint32_t PacketTracker::getResendCount() const { return _resendCount; }

std::uint32_t PacketTracker::getDroppedCount() const { return _droppedCount; }

std::uint32_t PacketTracker::getCriticalLossCount() const {
  return _criticalLosses;
}

std::chrono::milliseconds PacketTracker::getRoundTripTime() const {
  return std::chrono::duration_cast<std::chrono::milliseconds>(_roundTrip);
}
