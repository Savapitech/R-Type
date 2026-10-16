#pragma once

/**
 * @addtogroup api_network
 * @{
 */

#include <cstddef>

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
inline constexpr std::size_t PACKET_HEADER_SIZE = 16;
/// Room left for messages after the packet header, in bytes.
inline constexpr std::size_t MAX_MESSAGE_SIZE =
    MAX_PAYLOAD_SIZE - PACKET_HEADER_SIZE;

/** @} */
