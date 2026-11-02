#pragma once

#include <cstdint>

namespace rtype::client {

/**
 * @brief Network statistics ready for display by the client UI.
 */
struct ClientNetworkStats {

    /** @brief Latest measured round-trip time in milliseconds. */
    float pingMs = 0.0f;
    /** @brief Smoothed round-trip time in milliseconds. */
    float smoothPingMs = 0.0f;
    /** @brief Network jitter in milliseconds. */
    float jitterMs = 0.0f;
    /** @brief Incoming bandwidth in kibibytes per second. */
    double downloadKiBs = 0.0;
    /** @brief Outgoing bandwidth in kibibytes per second. */
    double uploadKiBs = 0.0;
    /** @brief Received datagrams per second. */
    double packetsInPerSecond = 0.0;
    /** @brief Sent datagrams per second. */
    double packetsOutPerSecond = 0.0;
    /** @brief Reliable retransmissions per second. */
    double retransmissionsPerSecond = 0.0;
    /** @brief Total retransmissions since the connection started. */
    std::uint64_t totalRetransmissions = 0;
    /** @brief Total duplicated datagrams. */
    std::uint64_t totalDuplicates = 0;
    /** @brief Total out-of-order datagrams. */
    std::uint64_t totalOutOfOrder = 0;
    /** @brief Total bytes received. */
    std::uint64_t totalBytesReceived = 0;
    /** @brief Total bytes sent. */
    std::uint64_t totalBytesSent = 0;
};

} //namespace rtype::client
