#pragma once

/**
 * @addtogroup api_network
 * @{
 */

#include <cstdint>

/**
 * @brief Delivery guarantee of a packet, 0 is the most important.
 *
 * Packets of a lower value are always sent before packets of a higher value.
 */
enum class PacketPriority : std::uint8_t {
  Critical = 0,  ///< Resent until acknowledged, too many losses close the link.
  Reliable = 1,  ///< Resent a limited number of times, then dropped.
  Unreliable = 2 ///< Sent once.
};

/** @} */
