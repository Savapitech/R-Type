#pragma once

/**
 * @addtogroup api_network
 * @{
 */

#include <chrono>
#include <cstddef>
#include <cstdint>

/// Largest IPv4 datagram every host is required to accept, in bytes.
inline constexpr std::size_t MAX_PACKET_SIZE = 576;
/// Size of an IPv4 header without options, in bytes.
inline constexpr std::size_t IP_HEADER_SIZE = 20;
/// Size of a UDP header, in bytes.
inline constexpr std::size_t UDP_HEADER_SIZE = 8;
/// Largest UDP payload sent or accepted by the network layer, in bytes.
inline constexpr std::size_t MAX_PAYLOAD_SIZE =
    MAX_PACKET_SIZE - IP_HEADER_SIZE - UDP_HEADER_SIZE;
/// Size of the PacketHeader that starts every connected packet, in bytes.
inline constexpr std::size_t PACKET_HEADER_SIZE = 19;
/// Room left for messages after the packet header, in bytes.
inline constexpr std::size_t MAX_MESSAGE_SIZE =
    MAX_PAYLOAD_SIZE - PACKET_HEADER_SIZE;

/// Minimal delay before a packet without ack is sent again.
inline constexpr std::chrono::milliseconds RESEND_TIMEOUT{100};
/// Number of resends of a Reliable (priority 1) packet before it is dropped.
inline constexpr std::uint32_t RELIABLE_MAX_RESENDS = 20;
/// Resends of a Critical (priority 0) packet after which it counts as a loss.
inline constexpr std::uint32_t CRITICAL_RESENDS_BEFORE_LOSS = 10;
/// Critical losses after which the connection is considered unstable.
inline constexpr std::uint32_t MAX_CRITICAL_LOSSES = 3;
/// Time without a new loss after which one Critical loss is forgiven, zero
/// keeps every loss for the whole connection.
inline constexpr std::chrono::milliseconds CRITICAL_LOSS_DECAY{30000};
/// Delay before an ack is sent alone when there is nothing else to send.
inline constexpr std::chrono::milliseconds ACK_ONLY_DELAY{50};
/// Period of the timer that flushes the outgoing queues.
inline constexpr std::chrono::milliseconds FLUSH_INTERVAL{5};

/** @} */
