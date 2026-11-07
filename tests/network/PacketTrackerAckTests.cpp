#include "Test.hpp"

#include <cstdint>

#include "tracking/PacketTracker.hpp"

// Sequence numbers

TEST(PacketTrackerSequence, StartsAtOne) {
  PacketTracker tracker;

  CHECK_EQ(tracker.getNextSequence(), 1u);
}

TEST(PacketTrackerSequence, IncreasesByOne) {
  PacketTracker tracker;

  for (std::uint32_t expected = 1; expected <= 100; ++expected)
    CHECK_EQ(tracker.getNextSequence(), expected);
}

TEST(PacketTrackerSequence, TrackersAreIndependent) {
  PacketTracker first;
  PacketTracker second;

  first.getNextSequence();
  first.getNextSequence();
  CHECK_EQ(second.getNextSequence(), 1u);
  CHECK_EQ(first.getNextSequence(), 3u);
}

// Receive window

TEST(PacketTrackerReceive, NoAckBeforeAnyPacket) {
  const PacketTracker tracker;

  CHECK_EQ(tracker.getAck(), 0u);
  CHECK_EQ(tracker.getAckBits(), 0u);
}

TEST(PacketTrackerReceive, FirstPacketIsNew) {
  PacketTracker tracker;

  CHECK_EQ(tracker.onPacketReceived(10u), PacketReceiveRes::New);
  CHECK_EQ(tracker.getAck(), 10u);
  CHECK_EQ(tracker.getAckBits(), 0u);
}

TEST(PacketTrackerReceive, NextPacketSetsFirstBit) {
  PacketTracker tracker;

  tracker.onPacketReceived(1u);
  CHECK_EQ(tracker.onPacketReceived(2u), PacketReceiveRes::New);
  CHECK_EQ(tracker.getAck(), 2u);
  CHECK_EQ(tracker.getAckBits(), 0x1u);
}

TEST(PacketTrackerReceive, ConsecutivePacketsFillBits) {
  PacketTracker tracker;

  for (std::uint32_t sequence = 1; sequence <= 5; ++sequence)
    tracker.onPacketReceived(sequence);
  CHECK_EQ(tracker.getAck(), 5u);
  CHECK_EQ(tracker.getAckBits(), 0xFu);
}

TEST(PacketTrackerReceive, GapLeavesZeroBits) {
  PacketTracker tracker;

  tracker.onPacketReceived(1u);
  tracker.onPacketReceived(4u);
  CHECK_EQ(tracker.getAck(), 4u);
  CHECK_EQ(tracker.getAckBits(), 0x4u);
}

TEST(PacketTrackerReceive, DuplicateOfLatestIsDup) {
  PacketTracker tracker;

  tracker.onPacketReceived(7u);
  CHECK_EQ(tracker.onPacketReceived(7u), PacketReceiveRes::Dup);
  CHECK_EQ(tracker.getDuplicateCount(), 1u);
}

TEST(PacketTrackerReceive, LatePacketIsOut) {
  PacketTracker tracker;

  tracker.onPacketReceived(1u);
  tracker.onPacketReceived(3u);
  CHECK_EQ(tracker.onPacketReceived(2u), PacketReceiveRes::Out);
  CHECK_EQ(tracker.getOutOfOrderCount(), 1u);
  CHECK_EQ(tracker.getAck(), 3u);
  CHECK_EQ(tracker.getAckBits(), 0x3u);
}

TEST(PacketTrackerReceive, LatePacketReceivedTwiceIsDup) {
  PacketTracker tracker;

  tracker.onPacketReceived(1u);
  tracker.onPacketReceived(3u);
  tracker.onPacketReceived(2u);
  CHECK_EQ(tracker.onPacketReceived(2u), PacketReceiveRes::Dup);
  CHECK_EQ(tracker.getDuplicateCount(), 1u);
  CHECK_EQ(tracker.getOutOfOrderCount(), 1u);
}

TEST(PacketTrackerReceive, OldPacketAlreadyInBitsIsDup) {
  PacketTracker tracker;

  tracker.onPacketReceived(1u);
  tracker.onPacketReceived(2u);
  CHECK_EQ(tracker.onPacketReceived(1u), PacketReceiveRes::Dup);
}

TEST(PacketTrackerReceive, PacketThirtyTwoBehindIsStillTracked) {
  PacketTracker tracker;

  tracker.onPacketReceived(40u);
  CHECK_EQ(tracker.onPacketReceived(8u), PacketReceiveRes::Out);
  CHECK_EQ(tracker.getAckBits(), 1u << 31);
}

TEST(PacketTrackerReceive, PacketThirtyThreeBehindIsTooOld) {
  PacketTracker tracker;

  tracker.onPacketReceived(40u);
  CHECK_EQ(tracker.onPacketReceived(7u), PacketReceiveRes::TooOld);
  CHECK_EQ(tracker.getTooOldCount(), 1u);
  CHECK_EQ(tracker.getAckBits(), 0u);
}

TEST(PacketTrackerReceive, JumpOfThirtyTwoKeepsOnlyTheOldestBit) {
  PacketTracker tracker;

  tracker.onPacketReceived(1u);
  tracker.onPacketReceived(2u);
  tracker.onPacketReceived(34u);
  CHECK_EQ(tracker.getAck(), 34u);
  CHECK_EQ(tracker.getAckBits(), 1u << 31);
}

TEST(PacketTrackerReceive, JumpPastTheWindowClearsBits) {
  PacketTracker tracker;

  for (std::uint32_t sequence = 1; sequence <= 10; ++sequence)
    tracker.onPacketReceived(sequence);
  CHECK_EQ(tracker.onPacketReceived(100u), PacketReceiveRes::New);
  CHECK_EQ(tracker.getAck(), 100u);
  CHECK_EQ(tracker.getAckBits(), 0u);
}

TEST(PacketTrackerReceive, BitsShiftWithNewPackets) {
  PacketTracker tracker;

  tracker.onPacketReceived(1u);
  tracker.onPacketReceived(2u);
  tracker.onPacketReceived(5u);
  // 4 and 3 missing, 2 and 1 received
  CHECK_EQ(tracker.getAckBits(), 0xCu);
}

TEST(PacketTrackerReceive, FirstPacketCanBeAnySequence) {
  PacketTracker tracker;

  CHECK_EQ(tracker.onPacketReceived(0xFFFF0000u), PacketReceiveRes::New);
  CHECK_EQ(tracker.getAck(), 0xFFFF0000u);
}

TEST(PacketTrackerReceive, CountersStartAtZero) {
  const PacketTracker tracker;

  CHECK_EQ(tracker.getDuplicateCount(), 0u);
  CHECK_EQ(tracker.getOutOfOrderCount(), 0u);
  CHECK_EQ(tracker.getTooOldCount(), 0u);
  CHECK_EQ(tracker.getResendCount(), 0u);
  CHECK_EQ(tracker.getDroppedCount(), 0u);
  CHECK_EQ(tracker.getCriticalLossCount(), 0u);
  CHECK_FALSE(tracker.isTimedOut());
}

TEST(PacketTrackerReceive, CountsManyDuplicates) {
  PacketTracker tracker;

  tracker.onPacketReceived(3u);
  for (int i = 0; i < 5; ++i)
    tracker.onPacketReceived(3u);
  CHECK_EQ(tracker.getDuplicateCount(), 5u);
}

TEST(PacketTrackerReceive, OutOfOrderFillsEveryHole) {
  PacketTracker tracker;

  tracker.onPacketReceived(1u);
  tracker.onPacketReceived(6u);
  for (std::uint32_t sequence = 2; sequence <= 5; ++sequence)
    CHECK_EQ(tracker.onPacketReceived(sequence), PacketReceiveRes::Out);
  CHECK_EQ(tracker.getAckBits(), 0x1Fu);
  CHECK_EQ(tracker.getOutOfOrderCount(), 4u);
}

// isPacketinAck

TEST(PacketTrackerIsInAck, AckItselfIsAcknowledged) {
  CHECK(PacketTracker::isPacketinAck(10, 10, 0));
}

TEST(PacketTrackerIsInAck, NewerThanAckIsNot) {
  CHECK_FALSE(PacketTracker::isPacketinAck(11, 10, 0xFFFFFFFF));
}

TEST(PacketTrackerIsInAck, PreviousNeedsFirstBit) {
  CHECK(PacketTracker::isPacketinAck(9, 10, 0x1));
  CHECK_FALSE(PacketTracker::isPacketinAck(9, 10, 0x2));
}

TEST(PacketTrackerIsInAck, ThirtyTwoBehindUsesLastBit) {
  CHECK(PacketTracker::isPacketinAck(8, 40, 1u << 31));
  CHECK_FALSE(PacketTracker::isPacketinAck(8, 40, 0x7FFFFFFF));
}

TEST(PacketTrackerIsInAck, PastTheWindowIsNot) {
  CHECK_FALSE(PacketTracker::isPacketinAck(7, 40, 0xFFFFFFFF));
}

TEST(PacketTrackerIsInAck, MatchesTheReceiverBits) {
  PacketTracker receiver;
  receiver.onPacketReceived(1u);
  receiver.onPacketReceived(3u);
  receiver.onPacketReceived(4u);
  const auto ack = receiver.getAck();
  const auto bits = receiver.getAckBits();

  CHECK(PacketTracker::isPacketinAck(1, ack, bits));
  CHECK_FALSE(PacketTracker::isPacketinAck(2, ack, bits));
  CHECK(PacketTracker::isPacketinAck(3, ack, bits));
  CHECK(PacketTracker::isPacketinAck(4, ack, bits));
  CHECK_FALSE(PacketTracker::isPacketinAck(5, ack, bits));
}
