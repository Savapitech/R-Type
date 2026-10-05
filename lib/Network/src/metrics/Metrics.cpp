#include "Metrics.hpp"

#include <cstddef>
#include <cstdint>
#include <cmath>

void updateRttMetrics(NetworkMetrics &metrics, float newRtt) {
  metrics.rtt = newRtt;


  if (metrics.smoothRtt != 0.0f) {
    metrics.smoothRtt = metrics.smoothRtt + 0.1f * (newRtt - metrics.smoothRtt);
  } else {
    metrics.smoothRtt = newRtt;
  }

  if (metrics.previousRtt != 0.0f) {
    const float delta = std::abs(newRtt - metrics.previousRtt);
    metrics.jitter = metrics.jitter + 0.1f * (delta - metrics.jitter);
  }

  metrics.previousRtt = newRtt;

}

void updatePacketMetrics(NetworkMetrics &metrics) {
  const uint64_t total = metrics.packetsReceived + metrics.packetsLost;

  if (total == 0) {
    metrics.packetLossPercentage = 0.0f;
    return;
  }

  metrics.packetLossPercentage = static_cast<float>(metrics.packetsLost) /
    static_cast<float>(total) * 100.0f;
}

void onPacketSend(NetworkMetrics &metrics, std::size_t packetSize) {
  ++metrics.packetsSent;
  metrics.bytesSent += packetSize;
}

void onPacketReceived(NetworkMetrics &metrics, std::size_t packetSize) {
  ++metrics.packetsReceived;
  metrics.bytesReceived += packetSize;
}

void onPacketDuplicated(NetworkMetrics &metrics) {
  ++metrics.packetsDuplicated;
}

void onPacketOut(NetworkMetrics &metrics) {
  ++metrics.packetsOut;
}

void onRetransmission(NetworkMetrics &metrics) {
  ++metrics.retransmissions;
}
