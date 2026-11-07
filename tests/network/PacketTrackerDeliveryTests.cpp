#include "NetworkTestUtils.hpp"
#include "Test.hpp"

#include <chrono>
#include <cstdint>
#include <vector>

#include "config/NetworkConfig.hpp"
#include "tracking/PacketTracker.hpp"

using rtype::test::at;
using rtype::test::bodyOf;
using rtype::test::headerOf;
using rtype::test::noise;
using rtype::test::peerHeader;

namespace {

constexpr std::uint32_t CONNECTION_ID = 7;
/// Too big for two of them to share a packet.
constexpr std::size_t BIG = 300;

using Packets = std::vector<std::vector<std::uint8_t>>;

Packets collect(PacketTracker &tracker, long long ms) {
  return tracker.collectOutgoing(at(ms), CONNECTION_ID);
}

/// Acknowledges `sequence` as the peer would, with a packet of its own.
void ack(PacketTracker &tracker, std::uint32_t peerSequence,
         std::uint32_t sequence, long long ms, std::uint32_t ackBits = 0) {
  tracker.onPacketReceived(peerHeader(peerSequence, sequence, ackBits), at(ms));
}

/// Sends a Reliable packet at `ms` and returns its header.
PacketHeader sendReliable(PacketTracker &tracker, long long ms,
                          std::uint32_t seed = 1) {
  tracker.enqueue(noise(32, seed), PacketPriority::Reliable);
  const auto packets = collect(tracker, ms);
  return headerOf(packets.front());
}

} // namespace

// Queues

TEST(PacketTrackerQueue, NothingQueuedSendsNothing) {
  PacketTracker tracker;

  CHECK(collect(tracker, 0).empty());
  CHECK(collect(tracker, 1000).empty());
}

TEST(PacketTrackerQueue, AcceptsMaxMessageSize) {
  PacketTracker tracker;

  CHECK(tracker.enqueue(noise(MAX_MESSAGE_SIZE)));
}

TEST(PacketTrackerQueue, RefusesOversizedPayload) {
  PacketTracker tracker;

  CHECK_FALSE(tracker.enqueue(noise(MAX_MESSAGE_SIZE + 1)));
  CHECK(collect(tracker, 0).empty());
}

TEST(PacketTrackerQueue, RefusesPastQueueLimit) {
  PacketTracker tracker;

  for (std::size_t i = 0; i < MAX_QUEUED_PACKETS; ++i)
    CHECK(tracker.enqueue({1}, PacketPriority::Unreliable));
  CHECK_FALSE(tracker.enqueue({1}, PacketPriority::Unreliable));
}

TEST(PacketTrackerQueue, EachPriorityHasItsOwnQueue) {
  PacketTracker tracker;

  for (std::size_t i = 0; i < MAX_QUEUED_PACKETS; ++i)
    tracker.enqueue({1}, PacketPriority::Unreliable);
  CHECK(tracker.enqueue({1}, PacketPriority::Reliable));
  CHECK(tracker.enqueue({1}, PacketPriority::Critical));
}

TEST(PacketTrackerQueue, DefaultPriorityIsUnreliable) {
  PacketTracker tracker;

  tracker.enqueue(noise(16));
  const auto packets = collect(tracker, 0);
  CHECK_EQ(packets.size(), 1u);
  CHECK_EQ(headerOf(packets[0]).priority, PacketPriority::Unreliable);
}

TEST(PacketTrackerQueue, CollectEmptiesTheQueues) {
  PacketTracker tracker;

  tracker.enqueue(noise(16));
  CHECK_EQ(collect(tracker, 0).size(), 1u);
  CHECK(collect(tracker, 1).empty());
}

// Packet layout

TEST(PacketTrackerPacket, UnreliableHeader) {
  PacketTracker tracker;

  tracker.enqueue(noise(16), PacketPriority::Unreliable);
  const auto header = headerOf(collect(tracker, 0).front());
  CHECK_EQ(header.connectionId, CONNECTION_ID);
  CHECK_EQ(header.sequence, 1u);
  CHECK_EQ(header.priority, PacketPriority::Unreliable);
  CHECK_EQ(header.reliableId, 0);
}

TEST(PacketTrackerPacket, BodyIsThePayload) {
  PacketTracker tracker;
  const auto payload = noise(64, 3);

  tracker.enqueue(payload);
  const auto packet = collect(tracker, 0).front();
  CHECK_EQ(packet.size(), PacketHeader::HEADER_SIZE + payload.size());
  CHECK(bodyOf(packet) == payload);
}

TEST(PacketTrackerPacket, ReliableIdsStartAtOne) {
  PacketTracker tracker;

  tracker.enqueue(noise(BIG, 1), PacketPriority::Reliable);
  tracker.enqueue(noise(BIG, 2), PacketPriority::Reliable);
  const auto packets = collect(tracker, 0);
  CHECK_EQ(packets.size(), 2u);
  CHECK_EQ(headerOf(packets[0]).reliableId, 1);
  CHECK_EQ(headerOf(packets[1]).reliableId, 2);
}

TEST(PacketTrackerPacket, CriticalAndReliableShareTheIds) {
  PacketTracker tracker;

  tracker.enqueue(noise(16, 1), PacketPriority::Critical);
  tracker.enqueue(noise(16, 2), PacketPriority::Reliable);
  const auto packets = collect(tracker, 0);
  CHECK_EQ(packets.size(), 2u);
  CHECK_EQ(headerOf(packets[0]).reliableId, 1);
  CHECK_EQ(headerOf(packets[1]).reliableId, 2);
}

TEST(PacketTrackerPacket, SequencesIncrease) {
  PacketTracker tracker;

  for (std::uint32_t i = 1; i <= 5; ++i) {
    tracker.enqueue(noise(16, i));
    CHECK_EQ(headerOf(collect(tracker, i).front()).sequence, i);
  }
}

TEST(PacketTrackerPacket, NeverLargerThanMaxPayload) {
  PacketTracker tracker;

  for (std::uint32_t i = 0; i < 40; ++i)
    tracker.enqueue(noise(100 + i, i));
  for (const auto &packet : collect(tracker, 0))
    CHECK(packet.size() <= MAX_PAYLOAD_SIZE);
}

TEST(PacketTrackerPacket, CarriesTheAckOfTheReceivedPackets) {
  PacketTracker tracker;
  tracker.onPacketReceived(peerHeader(3), at(0));
  tracker.onPacketReceived(peerHeader(4), at(0));

  tracker.enqueue(noise(16));
  const auto header = headerOf(collect(tracker, 0).front());
  CHECK_EQ(header.ack, 4u);
  CHECK_EQ(header.ackBits, 0x1u);
}

// Batching

TEST(PacketTrackerBatch, SmallPayloadsShareAPacket) {
  PacketTracker tracker;
  const auto first = noise(20, 1);
  const auto second = noise(30, 2);
  const auto third = noise(40, 3);

  tracker.enqueue(first);
  tracker.enqueue(second);
  tracker.enqueue(third);
  const auto packets = collect(tracker, 0);
  CHECK_EQ(packets.size(), 1u);

  std::vector<std::uint8_t> expected = first;
  expected.insert(expected.end(), second.begin(), second.end());
  expected.insert(expected.end(), third.begin(), third.end());
  CHECK(bodyOf(packets[0]) == expected);
}

TEST(PacketTrackerBatch, FillsUpToMaxMessageSize) {
  PacketTracker tracker;

  tracker.enqueue(noise(200, 1));
  tracker.enqueue(noise(200, 2));
  tracker.enqueue(noise(MAX_MESSAGE_SIZE - 400, 3));
  const auto packets = collect(tracker, 0);
  CHECK_EQ(packets.size(), 1u);
  CHECK_EQ(packets[0].size(), MAX_PAYLOAD_SIZE);
}

TEST(PacketTrackerBatch, SplitsWhenTheNextDoesNotFit) {
  PacketTracker tracker;

  tracker.enqueue(noise(200, 1));
  tracker.enqueue(noise(200, 2));
  tracker.enqueue(noise(MAX_MESSAGE_SIZE - 399, 3));
  CHECK_EQ(collect(tracker, 0).size(), 2u);
}

TEST(PacketTrackerBatch, KeepsQueueOrderAcrossPackets) {
  PacketTracker tracker;
  const auto first = noise(BIG, 1);
  const auto second = noise(BIG, 2);

  tracker.enqueue(first);
  tracker.enqueue(second);
  const auto packets = collect(tracker, 0);
  CHECK_EQ(packets.size(), 2u);
  CHECK(bodyOf(packets[0]) == first);
  CHECK(bodyOf(packets[1]) == second);
}

TEST(PacketTrackerBatch, PrioritiesAreNeverMixed) {
  PacketTracker tracker;

  tracker.enqueue(noise(10, 1), PacketPriority::Unreliable);
  tracker.enqueue(noise(10, 2), PacketPriority::Reliable);
  tracker.enqueue(noise(10, 3), PacketPriority::Critical);
  const auto packets = collect(tracker, 0);
  CHECK_EQ(packets.size(), 3u);
}

TEST(PacketTrackerBatch, CriticalLeavesFirst) {
  PacketTracker tracker;

  tracker.enqueue(noise(10, 1), PacketPriority::Unreliable);
  tracker.enqueue(noise(10, 2), PacketPriority::Reliable);
  tracker.enqueue(noise(10, 3), PacketPriority::Critical);
  const auto packets = collect(tracker, 0);
  CHECK_EQ(headerOf(packets[0]).priority, PacketPriority::Critical);
  CHECK_EQ(headerOf(packets[1]).priority, PacketPriority::Reliable);
  CHECK_EQ(headerOf(packets[2]).priority, PacketPriority::Unreliable);
}

TEST(PacketTrackerBatch, ReliableBatchHasOneId) {
  PacketTracker tracker;

  tracker.enqueue(noise(10, 1), PacketPriority::Reliable);
  tracker.enqueue(noise(10, 2), PacketPriority::Reliable);
  const auto packets = collect(tracker, 0);
  CHECK_EQ(packets.size(), 1u);
  CHECK_EQ(headerOf(packets[0]).reliableId, 1);
}

// Resends

TEST(PacketTrackerResend, UnreliableIsSentOnce) {
  PacketTracker tracker;

  tracker.enqueue(noise(16), PacketPriority::Unreliable);
  CHECK_EQ(collect(tracker, 0).size(), 1u);
  CHECK(collect(tracker, 5000).empty());
  CHECK_EQ(tracker.getResendCount(), 0u);
}

TEST(PacketTrackerResend, ReliableWaitsForTheTimeout) {
  PacketTracker tracker;
  const auto timeout = RESEND_TIMEOUT.count();

  sendReliable(tracker, 0);
  CHECK(collect(tracker, timeout - 1).empty());
  CHECK_EQ(collect(tracker, timeout).size(), 1u);
  CHECK_EQ(tracker.getResendCount(), 1u);
}

TEST(PacketTrackerResend, KeepsIdAndBody) {
  PacketTracker tracker;

  tracker.enqueue(noise(48, 9), PacketPriority::Reliable);
  const auto original = collect(tracker, 0).front();
  const auto resent = collect(tracker, RESEND_TIMEOUT.count()).front();
  CHECK_EQ(headerOf(resent).reliableId, headerOf(original).reliableId);
  CHECK_EQ(headerOf(resent).priority, PacketPriority::Reliable);
  CHECK(bodyOf(resent) == bodyOf(original));
}

TEST(PacketTrackerResend, UsesANewSequence) {
  PacketTracker tracker;

  const auto original = sendReliable(tracker, 0);
  const auto resent = headerOf(collect(tracker, 100).front());
  CHECK(resent.sequence > original.sequence);
}

TEST(PacketTrackerResend, WaitsAgainAfterAResend) {
  PacketTracker tracker;

  sendReliable(tracker, 0);
  CHECK_EQ(collect(tracker, 100).size(), 1u);
  CHECK(collect(tracker, 150).empty());
  CHECK_EQ(collect(tracker, 200).size(), 1u);
  CHECK_EQ(tracker.getResendCount(), 2u);
}

TEST(PacketTrackerResend, AckedPacketIsNotResent) {
  PacketTracker tracker;

  const auto sent = sendReliable(tracker, 0);
  ack(tracker, 1, sent.sequence, 10);
  CHECK(collect(tracker, 500).empty());
}

TEST(PacketTrackerResend, AckBitsAcknowledgeOlderPackets) {
  PacketTracker tracker;

  const auto first = sendReliable(tracker, 0, 1);
  const auto second = sendReliable(tracker, 0, 2);
  CHECK_EQ(second.sequence, first.sequence + 1);
  ack(tracker, 1, second.sequence, 10, 0x1);
  CHECK(collect(tracker, 500).empty());
}

TEST(PacketTrackerResend, OnlyTheAckedPacketIsCleared) {
  PacketTracker tracker;

  const auto first = sendReliable(tracker, 0, 1);
  sendReliable(tracker, 0, 2);
  ack(tracker, 1, first.sequence, 10);
  const auto packets = collect(tracker, 100);
  CHECK_EQ(packets.size(), 1u);
  CHECK_EQ(headerOf(packets[0]).reliableId, 2);
}

TEST(PacketTrackerResend, AckOfTheResendClearsThePacket) {
  PacketTracker tracker;

  sendReliable(tracker, 0);
  const auto resent = headerOf(collect(tracker, 100).front());
  ack(tracker, 1, resent.sequence, 120);
  CHECK(collect(tracker, 1000).empty());
}

TEST(PacketTrackerResend, LateAckOfTheFirstSendClearsThePacket) {
  PacketTracker tracker;

  const auto original = sendReliable(tracker, 0);
  collect(tracker, 100);
  ack(tracker, 1, original.sequence, 120);
  CHECK(collect(tracker, 1000).empty());
}

TEST(PacketTrackerResend, AckFromADuplicatePacketStillCounts) {
  PacketTracker tracker;

  const auto sent = sendReliable(tracker, 0);
  tracker.onPacketReceived(peerHeader(1), at(5));
  CHECK_FALSE(tracker.onPacketReceived(peerHeader(1, sent.sequence), at(10)));
  CHECK(collect(tracker, 500).empty());
}

TEST(PacketTrackerResend, OldestPacketIsResentFirst) {
  PacketTracker tracker;

  tracker.enqueue(noise(BIG, 1), PacketPriority::Reliable);
  tracker.enqueue(noise(BIG, 2), PacketPriority::Reliable);
  collect(tracker, 0);
  const auto packets = collect(tracker, 100);
  CHECK_EQ(packets.size(), 2u);
  CHECK_EQ(headerOf(packets[0]).reliableId, 1);
  CHECK_EQ(headerOf(packets[1]).reliableId, 2);
}

TEST(PacketTrackerResend, CriticalResendsComeFirst) {
  PacketTracker tracker;

  tracker.enqueue(noise(16, 1), PacketPriority::Reliable);
  collect(tracker, 0);
  tracker.enqueue(noise(16, 2), PacketPriority::Critical);
  collect(tracker, 1);
  const auto packets = collect(tracker, 101);
  CHECK_EQ(packets.size(), 2u);
  CHECK_EQ(headerOf(packets[0]).priority, PacketPriority::Critical);
  CHECK_EQ(headerOf(packets[1]).priority, PacketPriority::Reliable);
}

TEST(PacketTrackerResend, ReliableIsDroppedAfterMaxResends) {
  PacketTracker tracker;
  const auto timeout = RESEND_TIMEOUT.count();

  sendReliable(tracker, 0);
  for (std::uint32_t i = 1; i <= RELIABLE_MAX_RESENDS; ++i)
    CHECK_EQ(collect(tracker, i * timeout).size(), 1u);
  CHECK(collect(tracker, (RELIABLE_MAX_RESENDS + 1) * timeout).empty());
  CHECK_EQ(tracker.getDroppedCount(), 1u);
  CHECK_EQ(tracker.getResendCount(), RELIABLE_MAX_RESENDS);
}

TEST(PacketTrackerResend, DroppedPacketIsGoneForGood) {
  PacketTracker tracker;
  const auto timeout = RESEND_TIMEOUT.count();

  sendReliable(tracker, 0);
  for (std::uint32_t i = 1; i <= RELIABLE_MAX_RESENDS + 1; ++i)
    collect(tracker, i * timeout);
  CHECK(collect(tracker, 100000).empty());
  CHECK_EQ(tracker.getDroppedCount(), 1u);
}

TEST(PacketTrackerResend, CriticalIsNeverDropped) {
  PacketTracker tracker;
  const auto timeout = RESEND_TIMEOUT.count();

  tracker.enqueue(noise(16), PacketPriority::Critical);
  collect(tracker, 0);
  for (std::uint32_t i = 1; i <= RELIABLE_MAX_RESENDS + 10; ++i)
    CHECK_EQ(collect(tracker, i * timeout).size(), 1u);
  CHECK_EQ(tracker.getDroppedCount(), 0u);
}

// Critical losses

TEST(PacketTrackerCritical, LossCountedAfterTooManyResends) {
  PacketTracker tracker;
  const auto timeout = RESEND_TIMEOUT.count();

  tracker.enqueue(noise(16), PacketPriority::Critical);
  collect(tracker, 0);
  for (std::uint32_t i = 1; i <= CRITICAL_RESENDS_BEFORE_LOSS; ++i)
    collect(tracker, i * timeout);
  CHECK_EQ(tracker.getCriticalLossCount(), 0u);
  collect(tracker, (CRITICAL_RESENDS_BEFORE_LOSS + 1) * timeout);
  CHECK_EQ(tracker.getCriticalLossCount(), 1u);
}

TEST(PacketTrackerCritical, APacketCountsOnlyOnce) {
  PacketTracker tracker;
  const auto timeout = RESEND_TIMEOUT.count();

  tracker.enqueue(noise(16), PacketPriority::Critical);
  collect(tracker, 0);
  for (std::uint32_t i = 1; i <= CRITICAL_RESENDS_BEFORE_LOSS * 2; ++i)
    collect(tracker, i * timeout);
  CHECK_EQ(tracker.getCriticalLossCount(), 1u);
  CHECK_FALSE(tracker.isTimedOut());
}

TEST(PacketTrackerCritical, TooManyLossesTimeOut) {
  PacketTracker tracker;
  const auto timeout = RESEND_TIMEOUT.count();

  for (std::uint32_t i = 0; i < MAX_CRITICAL_LOSSES; ++i)
    tracker.enqueue(noise(BIG, i + 1), PacketPriority::Critical);
  CHECK_EQ(collect(tracker, 0).size(), MAX_CRITICAL_LOSSES);
  for (std::uint32_t i = 1; i <= CRITICAL_RESENDS_BEFORE_LOSS + 1; ++i)
    collect(tracker, i * timeout);
  CHECK_EQ(tracker.getCriticalLossCount(), MAX_CRITICAL_LOSSES);
  CHECK(tracker.isTimedOut());
}

TEST(PacketTrackerCritical, ReliableResendsAreNotLosses) {
  PacketTracker tracker;
  const auto timeout = RESEND_TIMEOUT.count();

  sendReliable(tracker, 0);
  for (std::uint32_t i = 1; i <= RELIABLE_MAX_RESENDS + 1; ++i)
    collect(tracker, i * timeout);
  CHECK_EQ(tracker.getCriticalLossCount(), 0u);
}

TEST(PacketTrackerCritical, LossIsForgivenAfterTheDecay) {
  PacketTracker tracker;
  const auto timeout = RESEND_TIMEOUT.count();
  const long long lossAt = (CRITICAL_RESENDS_BEFORE_LOSS + 1) * timeout;

  tracker.enqueue(noise(16), PacketPriority::Critical);
  collect(tracker, 0);
  for (std::uint32_t i = 1; i <= CRITICAL_RESENDS_BEFORE_LOSS + 1; ++i)
    collect(tracker, i * timeout);
  CHECK_EQ(tracker.getCriticalLossCount(), 1u);
  collect(tracker, lossAt + CRITICAL_LOSS_DECAY.count() - 1);
  CHECK_EQ(tracker.getCriticalLossCount(), 1u);
  collect(tracker, lossAt + CRITICAL_LOSS_DECAY.count());
  CHECK_EQ(tracker.getCriticalLossCount(), 0u);
}

TEST(PacketTrackerCritical, AckedCriticalIsNotALoss) {
  PacketTracker tracker;

  tracker.enqueue(noise(16), PacketPriority::Critical);
  const auto sent = headerOf(collect(tracker, 0).front());
  ack(tracker, 1, sent.sequence, 20);
  for (std::uint32_t i = 1; i <= CRITICAL_RESENDS_BEFORE_LOSS + 5; ++i)
    CHECK(collect(tracker, i * 100).empty());
  CHECK_EQ(tracker.getCriticalLossCount(), 0u);
}

// Pending limit

TEST(PacketTrackerPending, StopsSendingReliableWhenFull) {
  PacketTracker tracker;
  std::uint32_t seed = 1;

  for (std::size_t sent = 0; sent < MAX_PENDING_PACKETS;
       sent += MAX_QUEUED_PACKETS) {
    for (std::size_t i = 0; i < MAX_QUEUED_PACKETS; ++i)
      tracker.enqueue(noise(BIG, seed++), PacketPriority::Reliable);
    CHECK_EQ(collect(tracker, 0).size(), MAX_QUEUED_PACKETS);
  }
  tracker.enqueue(noise(BIG, seed++), PacketPriority::Reliable);
  CHECK(collect(tracker, 0).empty());
}

TEST(PacketTrackerPending, UnreliableStillLeavesWhenFull) {
  PacketTracker tracker;
  std::uint32_t seed = 1;

  for (std::size_t sent = 0; sent < MAX_PENDING_PACKETS;
       sent += MAX_QUEUED_PACKETS) {
    for (std::size_t i = 0; i < MAX_QUEUED_PACKETS; ++i)
      tracker.enqueue(noise(BIG, seed++), PacketPriority::Reliable);
    collect(tracker, 0);
  }
  tracker.enqueue(noise(16), PacketPriority::Unreliable);
  const auto packets = collect(tracker, 0);
  CHECK_EQ(packets.size(), 1u);
  CHECK_EQ(headerOf(packets[0]).priority, PacketPriority::Unreliable);
}

TEST(PacketTrackerPending, AckFreesRoomForTheQueue) {
  PacketTracker tracker;
  std::uint32_t seed = 1;
  std::uint32_t lastSequence = 0;

  for (std::size_t sent = 0; sent < MAX_PENDING_PACKETS;
       sent += MAX_QUEUED_PACKETS) {
    for (std::size_t i = 0; i < MAX_QUEUED_PACKETS; ++i)
      tracker.enqueue(noise(BIG, seed++), PacketPriority::Reliable);
    lastSequence = headerOf(collect(tracker, 0).back()).sequence;
  }
  tracker.enqueue(noise(BIG, seed++), PacketPriority::Reliable);
  CHECK(collect(tracker, 0).empty());
  ack(tracker, 1, lastSequence, 1);
  const auto packets = collect(tracker, 2);
  CHECK_EQ(packets.size(), 1u);
  CHECK_EQ(headerOf(packets[0]).priority, PacketPriority::Reliable);
}

// Acks sent alone

TEST(PacketTrackerAckOnly, SentAfterTheDelay) {
  PacketTracker tracker;
  const auto delay = ACK_ONLY_DELAY.count();

  tracker.onPacketReceived(peerHeader(5, 0, 0, PacketPriority::Reliable, 1),
                           at(0));
  CHECK(collect(tracker, delay - 1).empty());
  const auto packets = collect(tracker, delay);
  CHECK_EQ(packets.size(), 1u);
}

TEST(PacketTrackerAckOnly, HasAnEmptyBody) {
  PacketTracker tracker;

  tracker.onPacketReceived(peerHeader(5, 0, 0, PacketPriority::Reliable, 1),
                           at(0));
  const auto packet = collect(tracker, ACK_ONLY_DELAY.count()).front();
  const auto header = headerOf(packet);
  CHECK_EQ(packet.size(), PacketHeader::HEADER_SIZE);
  CHECK_EQ(header.ack, 5u);
  CHECK_EQ(header.priority, PacketPriority::Unreliable);
  CHECK_EQ(header.reliableId, 0);
}

TEST(PacketTrackerAckOnly, SentOnlyOnce) {
  PacketTracker tracker;

  tracker.onPacketReceived(peerHeader(5, 0, 0, PacketPriority::Reliable, 1),
                           at(0));
  CHECK_EQ(collect(tracker, 60).size(), 1u);
  CHECK(collect(tracker, 200).empty());
}

TEST(PacketTrackerAckOnly, NotSentForUnreliablePackets) {
  PacketTracker tracker;

  tracker.onPacketReceived(peerHeader(5), at(0));
  CHECK(collect(tracker, 1000).empty());
}

TEST(PacketTrackerAckOnly, NotNeededWhenDataLeaves) {
  PacketTracker tracker;

  tracker.onPacketReceived(peerHeader(5, 0, 0, PacketPriority::Critical, 1),
                           at(0));
  tracker.enqueue(noise(16));
  const auto packets = collect(tracker, 0);
  CHECK_EQ(packets.size(), 1u);
  CHECK_EQ(headerOf(packets[0]).ack, 5u);
  CHECK(collect(tracker, 100).empty());
}

TEST(PacketTrackerAckOnly, DelayStartsAtTheFirstUnackedPacket) {
  PacketTracker tracker;

  tracker.onPacketReceived(peerHeader(1, 0, 0, PacketPriority::Reliable, 1),
                           at(0));
  tracker.onPacketReceived(peerHeader(2, 0, 0, PacketPriority::Reliable, 2),
                           at(40));
  const auto packets = collect(tracker, ACK_ONLY_DELAY.count());
  CHECK_EQ(packets.size(), 1u);
  CHECK_EQ(headerOf(packets[0]).ack, 2u);
  CHECK_EQ(headerOf(packets[0]).ackBits, 0x1u);
}

// Reception of reliable packets

TEST(PacketTrackerReliableReceive, NewReliableIsAccepted) {
  PacketTracker tracker;

  CHECK(tracker.onPacketReceived(
      peerHeader(1, 0, 0, PacketPriority::Reliable, 1), at(0)));
}

TEST(PacketTrackerReliableReceive, ResendOfAReceivedPacketIsRefused) {
  PacketTracker tracker;

  CHECK(tracker.onPacketReceived(
      peerHeader(1, 0, 0, PacketPriority::Reliable, 1), at(0)));
  CHECK_FALSE(tracker.onPacketReceived(
      peerHeader(2, 0, 0, PacketPriority::Reliable, 1), at(100)));
}

TEST(PacketTrackerReliableReceive, IdsOutOfOrderAreAcceptedOnce) {
  PacketTracker tracker;

  CHECK(tracker.onPacketReceived(
      peerHeader(1, 0, 0, PacketPriority::Reliable, 3), at(0)));
  CHECK(tracker.onPacketReceived(
      peerHeader(2, 0, 0, PacketPriority::Reliable, 1), at(0)));
  CHECK(tracker.onPacketReceived(
      peerHeader(3, 0, 0, PacketPriority::Reliable, 2), at(0)));
  CHECK_FALSE(tracker.onPacketReceived(
      peerHeader(4, 0, 0, PacketPriority::Reliable, 2), at(0)));
}

TEST(PacketTrackerReliableReceive, DuplicateSequenceIsRefused) {
  PacketTracker tracker;

  tracker.onPacketReceived(peerHeader(1), at(0));
  CHECK_FALSE(tracker.onPacketReceived(peerHeader(1), at(1)));
}

TEST(PacketTrackerReliableReceive, UnreliableIsAccepted) {
  PacketTracker tracker;

  CHECK(tracker.onPacketReceived(peerHeader(1), at(0)));
  CHECK(tracker.onPacketReceived(peerHeader(2), at(0)));
}

TEST(PacketTrackerReliableReceive, TooOldSequenceIsRefused) {
  PacketTracker tracker;

  tracker.onPacketReceived(peerHeader(100), at(0));
  CHECK_FALSE(tracker.onPacketReceived(peerHeader(10), at(0)));
}

TEST(PacketTrackerReliableReceive, OutOfOrderUnreliableIsAccepted) {
  PacketTracker tracker;

  tracker.onPacketReceived(peerHeader(5), at(0));
  CHECK(tracker.onPacketReceived(peerHeader(4), at(0)));
}

TEST(PacketTrackerReliableReceive, CriticalIdsAreDeduplicatedToo) {
  PacketTracker tracker;

  CHECK(tracker.onPacketReceived(
      peerHeader(1, 0, 0, PacketPriority::Critical, 9), at(0)));
  CHECK_FALSE(tracker.onPacketReceived(
      peerHeader(2, 0, 0, PacketPriority::Critical, 9), at(0)));
}

// Round trip time

TEST(PacketTrackerRtt, ZeroBeforeTheFirstAck) {
  const PacketTracker tracker;

  CHECK_EQ(tracker.getRoundTripTime().count(), 0);
  CHECK_EQ(tracker.getRttSampleCount(), 0u);
  CHECK_EQ(tracker.getLastRttSample().count(), 0);
}

TEST(PacketTrackerRtt, MeasuredOnAck) {
  PacketTracker tracker;

  const auto sent = sendReliable(tracker, 0);
  ack(tracker, 1, sent.sequence, 40);
  CHECK_EQ(tracker.getRoundTripTime().count(), 40);
  CHECK_EQ(tracker.getLastRttSample().count(), 40000);
  CHECK_EQ(tracker.getRttSampleCount(), 1u);
}

TEST(PacketTrackerRtt, IsSmoothed) {
  PacketTracker tracker;

  const auto first = sendReliable(tracker, 0, 1);
  ack(tracker, 1, first.sequence, 40);
  const auto second = sendReliable(tracker, 1000, 2);
  ack(tracker, 2, second.sequence, 1080);
  // (40 * 7 + 80) / 8
  CHECK_EQ(tracker.getRoundTripTime().count(), 45);
  CHECK_EQ(tracker.getLastRttSample().count(), 80000);
  CHECK_EQ(tracker.getRttSampleCount(), 2u);
}

TEST(PacketTrackerRtt, SecondAckOfTheSamePacketIsIgnored) {
  PacketTracker tracker;

  const auto sent = sendReliable(tracker, 0);
  ack(tracker, 1, sent.sequence, 40);
  ack(tracker, 2, sent.sequence, 90);
  CHECK_EQ(tracker.getRoundTripTime().count(), 40);
  CHECK_EQ(tracker.getRttSampleCount(), 1u);
}

TEST(PacketTrackerRtt, LengthensTheResendDelay) {
  PacketTracker tracker;

  const auto first = sendReliable(tracker, 0, 1);
  ack(tracker, 1, first.sequence, 100);
  sendReliable(tracker, 1000, 2);
  // the delay is 1.5 times the round trip time once it exceeds the timeout
  CHECK(collect(tracker, 1100).empty());
  CHECK(collect(tracker, 1149).empty());
  CHECK_EQ(collect(tracker, 1150).size(), 1u);
}

TEST(PacketTrackerRtt, ShortRttKeepsTheMinimalTimeout) {
  PacketTracker tracker;

  const auto first = sendReliable(tracker, 0, 1);
  ack(tracker, 1, first.sequence, 10);
  sendReliable(tracker, 1000, 2);
  CHECK(collect(tracker, 1000 + RESEND_TIMEOUT.count() - 1).empty());
  CHECK_EQ(collect(tracker, 1000 + RESEND_TIMEOUT.count()).size(), 1u);
}
