#pragma once

/**
 * @file NetworkTestUtils.hpp
 * @brief Helpers shared by the network tests.
 */

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "protocol/MessageHeader.hpp"
#include "protocol/PacketHeader.hpp"
#include "protocol/PacketReader.hpp"
#include "protocol/PacketWriter.hpp"
#include "tracking/PacketPriority.hpp"

namespace rtype::test {

using Clock = std::chrono::steady_clock;
using std::chrono::milliseconds;

/// Fixed origin for the simulated clocks, far from the epoch.
inline Clock::time_point start() {
  return Clock::time_point{} + std::chrono::hours(1);
}

/// Time point `ms` milliseconds after start().
inline Clock::time_point at(long long ms) { return start() + milliseconds(ms); }

/// Deterministic bytes without any pattern, so no body ever gets smaller once
/// packed and the tests can compare the bytes that are sent.
inline std::vector<std::uint8_t> noise(std::size_t size,
                                       std::uint32_t seed = 1) {
  std::vector<std::uint8_t> bytes(size);
  std::uint32_t state = seed * 2654435761u + 0x9E3779B9u;
  for (auto &byte : bytes) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    byte = static_cast<std::uint8_t>(state >> 24);
  }
  return bytes;
}

/// A framed message: MessageHeader followed by the payload.
inline std::vector<std::uint8_t>
frame(MessageId type, const std::vector<std::uint8_t> &payload) {
  PacketWriter writer;
  MessageHeader header;
  header.setType(type);
  header.size = static_cast<std::uint16_t>(payload.size());
  header.serialize(writer);
  writer.writeBytes(payload.data(), payload.size());
  return writer.getData();
}

/// Header of a datagram built by a PacketTracker.
inline PacketHeader headerOf(const std::vector<std::uint8_t> &packet) {
  PacketReader reader(packet);
  PacketHeader header;
  header.deserialize(reader);
  return header;
}

/// Bytes that follow the PacketHeader of a datagram.
inline std::vector<std::uint8_t>
bodyOf(const std::vector<std::uint8_t> &packet) {
  if (packet.size() <= PacketHeader::HEADER_SIZE)
    return {};
  return {packet.begin() + PacketHeader::HEADER_SIZE, packet.end()};
}

/// Header of a packet sent by the peer, used to feed acks to a tracker.
inline PacketHeader
peerHeader(std::uint32_t sequence, std::uint32_t ack = 0,
           std::uint32_t ackBits = 0,
           PacketPriority priority = PacketPriority::Unreliable,
           std::uint16_t reliableId = 0) {
  PacketHeader header;
  header.connectionId = 1;
  header.sequence = sequence;
  header.ack = ack;
  header.ackBits = ackBits;
  header.priority = priority;
  header.reliableId = reliableId;
  return header;
}

/// Serialized datagram: PacketHeader followed by a body.
inline std::vector<std::uint8_t>
datagram(const PacketHeader &header, const std::vector<std::uint8_t> &body) {
  PacketWriter writer;
  header.serialize(writer);
  writer.writeBytes(body.data(), body.size());
  return writer.getData();
}

} // namespace rtype::test
