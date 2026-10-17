#pragma once

/**
 * @addtogroup api_network
 * @{
 */

#include <bitset>
#include <chrono>
#include <cstdint>
#include <deque>
#include <unordered_map>
#include <vector>

#include "../protocol/PacketHeader.hpp"
#include "PacketPriority.hpp"

/**
 * @brief Classification of a received packet sequence number.
 */
enum class PacketReceiveRes {
  New,   ///< Newer than every packet received so far.
  Dup,   ///< Already received.
  Out,   ///< Older than the latest one but inside the 32 packet window.
  TooOld ///< Older than the 32 packet window.
};

/**
 * @brief Generates outgoing sequence numbers, tracks received ones and
 * resends the packets that were not acknowledged.
 */
class PacketTracker {
public:
  /**
   * @brief Creates a tracker whose first sequence number is 1.
   */
  PacketTracker();

  /**
   * @brief Returns the sequence number for the next outgoing packet.
   * @return A sequence number, increased by one on each call.
   */
  std::uint32_t getNextSequence();

  /**
   * @brief Records a received sequence number.
   * @param sequence Sequence number read from the packet header.
   * @return How the packet relates to the ones already received.
   */
  PacketReceiveRes onPacketReceived(std::uint32_t sequence);

  /**
   * @brief Returns the latest received sequence number.
   * @return The latest sequence, or 0 if nothing was received.
   */
  std::uint32_t getAck() const;

  /**
   * @brief Returns the bitfield of the 32 sequences before the latest one.
   * @return Bit i is set when sequence `ack - 1 - i` was received.
   */
  std::uint32_t getAckBits() const;

  /**
   * @brief Tells whether an ack and its bitfield cover a sequence number.
   * @param sequence Sequence number to look for.
   * @param ack Latest sequence acknowledged by the peer.
   * @param ackBits Bitfield sent along with `ack`.
   * @return True if the peer acknowledged the sequence.
   */
  static bool isPacketinAck(std::uint32_t sequence, std::uint32_t ack,
                            std::uint32_t ackBits);

  /// Clock used by the resend logic.
  using TimePoint = std::chrono::steady_clock::time_point;

  /**
   * @brief Queues encoded messages for the next call to collectOutgoing().
   * @param payload Messages, headers included.
   * @param priority Delivery guarantee of the packet.
   * @return False if the payload is larger than MAX_MESSAGE_SIZE.
   */
  bool enqueue(std::vector<std::uint8_t> payload,
               PacketPriority priority = PacketPriority::Unreliable);

  /**
   * @brief Builds the datagrams to send now.
   *
   * Packets are ordered by priority, and for one priority the resends come
   * before the new packets. A packet that was waiting for an ack too long is
   * sent again with a new sequence number and the same reliable id. An empty
   * packet is added when an ack is due and nothing else carries it.
   * @param now Current time.
   * @param connectionId Connection id written in every header.
   * @return Complete datagrams, headers included.
   */
  std::vector<std::vector<std::uint8_t>>
  collectOutgoing(TimePoint now, std::uint32_t connectionId);

  /**
   * @brief Handles the header of a received packet.
   *
   * Acknowledged packets stop being resent, then the packet is checked for
   * duplicates, both by sequence and by reliable id.
   * @param header Header read from the datagram.
   * @param now Current time.
   * @return True if the messages of the packet must be processed.
   */
  bool onPacketReceived(const PacketHeader &header, TimePoint now);

  /**
   * @brief Tells whether too many Critical packets were lost.
   * @return True when the connection is too unstable and must be closed.
   */
  bool isTimedOut() const;

  /// Number of packets sent again so far.
  std::uint32_t getResendCount() const;
  /// Number of Reliable packets dropped after too many resends.
  std::uint32_t getDroppedCount() const;
  /// Number of Critical packets that counted as a loss.
  std::uint32_t getCriticalLossCount() const;
  /// Smoothed round trip time, zero until the first ack.
  std::chrono::milliseconds getRoundTripTime() const;

private:
  struct Pending {
    std::vector<std::uint8_t> payload;
    PacketPriority priority = PacketPriority::Unreliable;
    std::uint16_t reliableId = 0;
    std::uint64_t order = 0;
    TimePoint lastSend;
    std::uint32_t resends = 0;
    bool lossCounted = false;
    std::vector<std::uint32_t> sequences;
  };

  static constexpr std::size_t RELIABLE_WINDOW = 1024;
  static constexpr std::size_t MAX_TRACKED_SEQUENCES = 32;

  std::vector<std::uint8_t> buildPacket(std::uint32_t connectionId,
                                        PacketPriority priority,
                                        std::uint16_t reliableId,
                                        const std::vector<std::uint8_t> &body,
                                        std::uint32_t &sequence);
  void acknowledge(std::uint32_t ack, std::uint32_t ackBits, TimePoint now);
  bool markReliableReceived(std::uint16_t reliableId);
  std::chrono::milliseconds resendDelay() const;

  std::deque<std::vector<std::uint8_t>> _queues[3];
  std::unordered_map<std::uint16_t, Pending> _pending;
  std::uint16_t _nextReliableId = 1;
  std::uint64_t _nextOrder = 0;
  bool _ackDue = false;
  TimePoint _ackDueSince;
  bool _hasReliableReceived = false;
  std::uint16_t _highestReliable = 0;
  std::bitset<RELIABLE_WINDOW> _reliableSeen;
  std::uint32_t _resendCount = 0;
  std::uint32_t _droppedCount = 0;
  std::uint32_t _criticalLosses = 0;
  TimePoint _lastLossChange;
  std::chrono::microseconds _roundTrip{0};

  std::uint32_t _nextSequence;
  bool _hasReceivedPacket;
  std::uint32_t _lastReceived;
  std::uint32_t _receivedBits;
};

/** @} */
