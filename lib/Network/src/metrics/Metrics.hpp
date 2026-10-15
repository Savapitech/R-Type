#pragma once

/**
 * @addtogroup api_network
 * @{
 */

#include <cstddef>
#include <cstdint>

/**
 * @brief Traffic and latency counters of a network endpoint.
 */
struct NetworkMetrics {
  float rtt = 0.0f;         ///< Last round trip time sample.
  float smoothRtt = 0.0f;   ///< Exponentially smoothed round trip time.
  float previousRtt = 0.0f; ///< Round trip time sample before the last one.
  float jitter = 0.0f;      ///< Smoothed variation between samples.
  /// Lost packets over all packets, in percent.
  float packetLossPercentage = 0.0f;
  uint32_t retransmissions = 0; ///< Number of resent packets.

  uint64_t bytesSent = 0;     ///< Total bytes sent.
  uint64_t bytesReceived = 0; ///< Total bytes received.

  uint64_t packetsSent = 0;       ///< Total datagrams sent.
  uint64_t packetsReceived = 0;   ///< Total datagrams received.
  uint64_t packetsLost = 0;       ///< Datagrams known to be lost.
  uint64_t packetsDuplicated = 0; ///< Datagrams received twice.
  uint64_t packetsOut = 0;        ///< Datagrams received out of order.
};

/**
 * @brief Adds a round trip time sample and updates smoothing and jitter.
 * @param metrics Counters to update.
 * @param newRtt New round trip time sample.
 */
void updateRttMetrics(NetworkMetrics &metrics, float newRtt);

/**
 * @brief Recomputes the packet loss percentage.
 * @param metrics Counters to update.
 */
void updatePacketMetrics(NetworkMetrics &metrics);

/**
 * @brief Counts a sent datagram.
 * @param metrics Counters to update.
 * @param packetSize Size of the datagram in bytes.
 */
void onPacketSend(NetworkMetrics &metrics, std::size_t packetSize);

/**
 * @brief Counts a received datagram.
 * @param metrics Counters to update.
 * @param packetSize Size of the datagram in bytes.
 */
void onPacketReceived(NetworkMetrics &metrics, std::size_t packetSize);

/**
 * @brief Counts a duplicated datagram.
 * @param metrics Counters to update.
 */
void onPacketDuplicated(NetworkMetrics &metrics);

/**
 * @brief Counts a datagram received out of order.
 * @param metrics Counters to update.
 */
void onPacketOut(NetworkMetrics &metrics);

/**
 * @brief Counts a retransmitted datagram.
 * @param metrics Counters to update.
 */
void onRetransmission(NetworkMetrics &metrics);

/** @} */
