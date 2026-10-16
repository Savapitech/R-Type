#pragma once

/**
 * @addtogroup api_network
 * @{
 */

#include <cstdint>

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
 * @brief Generates outgoing sequence numbers and tracks received ones.
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

private:
  std::uint32_t _nextSequence;
  bool _hasReceivedPacket;
  std::uint32_t _lastReceived;
  std::uint32_t _receivedBits;
};

/** @} */
