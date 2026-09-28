#include <cstdint>

struct NetworkMetrics {
  float rtt = 0.0f;
  float smoothRtt = 0.0f;
  float previousRtt = 0.0f;
  float jitter = 0.0f;
  float packetLossPercentage = 0.0f;
  uint32_t retransmissions = 0;

  uint64_t bytesSent = 0;
  uint64_t bytesReceived = 0;

  uint64_t packetsSent = 0;
  uint64_t packetsReceived = 0;
  uint64_t packetsLost = 0;
  uint64_t packetsDuplicated = 0;
  uint64_t packetsOut = 0;
};
