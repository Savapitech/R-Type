#pragma once

#include "MessageHeader.hpp"

/**
 * @brief Message types handled by the network layer itself.
 */
namespace systemMessage {
/**
 * @addtogroup api_network
 * @{
 */

/// Latency probe sent by a client.
inline constexpr MessageId Ping = 0x03;
/// Client closes its connection.
inline constexpr MessageId Disconnect = 0x04;
/// Server answer to a Ping.
inline constexpr MessageId Pong = 0x86;

/** @} */
} // namespace systemMessage
