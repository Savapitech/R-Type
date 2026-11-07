#include "NetworkTestUtils.hpp"
#include "Test.hpp"

#include <chrono>
#include <cstdint>

#include "connection/Connection.hpp"
#include "metrics/Metrics.hpp"

// NetworkMetrics

TEST(Metrics, StartAtZero) {
  const NetworkMetrics metrics{};

  CHECK_EQ(metrics.rtt, 0.0f);
  CHECK_EQ(metrics.smoothRtt, 0.0f);
  CHECK_EQ(metrics.jitter, 0.0f);
  CHECK_EQ(metrics.packetLossPercentage, 0.0f);
  CHECK_EQ(metrics.packetsSent, 0u);
  CHECK_EQ(metrics.packetsReceived, 0u);
  CHECK_EQ(metrics.bytesSent, 0u);
  CHECK_EQ(metrics.bytesReceived, 0u);
}

TEST(Metrics, FirstRttSampleSetsEverything) {
  NetworkMetrics metrics{};

  updateRttMetrics(metrics, 50.0f);
  CHECK_EQ(metrics.rtt, 50.0f);
  CHECK_EQ(metrics.smoothRtt, 50.0f);
  CHECK_EQ(metrics.previousRtt, 50.0f);
  CHECK_EQ(metrics.jitter, 0.0f);
}

TEST(Metrics, SmoothRttMovesATenthOfTheWay) {
  NetworkMetrics metrics{};

  updateRttMetrics(metrics, 50.0f);
  updateRttMetrics(metrics, 150.0f);
  CHECK_EQ(metrics.rtt, 150.0f);
  CHECK_NEAR(metrics.smoothRtt, 60.0f, 1e-4);
}

TEST(Metrics, JitterFollowsTheVariation) {
  NetworkMetrics metrics{};

  updateRttMetrics(metrics, 50.0f);
  updateRttMetrics(metrics, 70.0f);
  CHECK_NEAR(metrics.jitter, 2.0f, 1e-4);
  updateRttMetrics(metrics, 50.0f);
  CHECK_NEAR(metrics.jitter, 3.8f, 1e-4);
}

TEST(Metrics, StableRttHasNoJitter) {
  NetworkMetrics metrics{};

  for (int i = 0; i < 20; ++i)
    updateRttMetrics(metrics, 30.0f);
  CHECK_NEAR(metrics.jitter, 0.0f, 1e-6);
  CHECK_NEAR(metrics.smoothRtt, 30.0f, 1e-4);
}

TEST(Metrics, PreviousRttIsTheLastSample) {
  NetworkMetrics metrics{};

  updateRttMetrics(metrics, 10.0f);
  updateRttMetrics(metrics, 20.0f);
  CHECK_EQ(metrics.previousRtt, 20.0f);
}

TEST(Metrics, SmoothRttConverges) {
  NetworkMetrics metrics{};

  updateRttMetrics(metrics, 10.0f);
  for (int i = 0; i < 200; ++i)
    updateRttMetrics(metrics, 100.0f);
  CHECK_NEAR(metrics.smoothRtt, 100.0f, 0.01);
}

TEST(Metrics, NoLossWithoutPackets) {
  NetworkMetrics metrics{};
  metrics.packetLossPercentage = 42.0f;

  updatePacketMetrics(metrics);
  CHECK_EQ(metrics.packetLossPercentage, 0.0f);
}

TEST(Metrics, LossPercentage) {
  NetworkMetrics metrics{};
  metrics.packetsReceived = 3;
  metrics.packetsLost = 1;

  updatePacketMetrics(metrics);
  CHECK_NEAR(metrics.packetLossPercentage, 25.0f, 1e-4);
}

TEST(Metrics, FullLoss) {
  NetworkMetrics metrics{};
  metrics.packetsLost = 8;

  updatePacketMetrics(metrics);
  CHECK_NEAR(metrics.packetLossPercentage, 100.0f, 1e-4);
}

TEST(Metrics, CountsSentPackets) {
  NetworkMetrics metrics{};

  onPacketSend(metrics, 100);
  onPacketSend(metrics, 28);
  CHECK_EQ(metrics.packetsSent, 2u);
  CHECK_EQ(metrics.bytesSent, 128u);
}

TEST(Metrics, CountsReceivedPackets) {
  NetworkMetrics metrics{};

  onPacketReceived(metrics, 19);
  onPacketReceived(metrics, 548);
  CHECK_EQ(metrics.packetsReceived, 2u);
  CHECK_EQ(metrics.bytesReceived, 567u);
}

TEST(Metrics, CountsDuplicatesOutOfOrderAndResends) {
  NetworkMetrics metrics{};

  onPacketDuplicated(metrics);
  onPacketDuplicated(metrics);
  onPacketOut(metrics);
  onRetransmission(metrics);
  onRetransmission(metrics);
  onRetransmission(metrics);
  CHECK_EQ(metrics.packetsDuplicated, 2u);
  CHECK_EQ(metrics.packetsOut, 1u);
  CHECK_EQ(metrics.retransmissions, 3u);
}

TEST(Metrics, SendAndReceiveAreIndependent) {
  NetworkMetrics metrics{};

  onPacketSend(metrics, 10);
  CHECK_EQ(metrics.packetsReceived, 0u);
  CHECK_EQ(metrics.bytesReceived, 0u);
}

// Connection

namespace {

asio::ip::udp::endpoint localEndpoint(std::uint16_t port) {
  return {asio::ip::make_address("127.0.0.1"), port};
}

} // namespace

TEST(Connection, KeepsIdAndEndpoint) {
  const Connection connection(12, localEndpoint(4242));

  CHECK_EQ(connection.getId(), 12u);
  CHECK(connection.getEndpoint() == localEndpoint(4242));
}

TEST(Connection, MakeHeaderUsesTheConnectionId) {
  Connection connection(5, localEndpoint(1));

  const auto header = connection.makeHeader();
  CHECK_EQ(header.connectionId, 5u);
  CHECK_EQ(header.sequence, 1u);
  CHECK_EQ(header.ack, 0u);
}

TEST(Connection, MakeHeaderIncreasesTheSequence) {
  Connection connection(5, localEndpoint(1));

  connection.makeHeader();
  connection.makeHeader();
  CHECK_EQ(connection.makeHeader().sequence, 3u);
}

TEST(Connection, MakeHeaderCarriesTheAck) {
  Connection connection(5, localEndpoint(1));

  connection.onPacketReceived(rtype::test::peerHeader(8));
  connection.onPacketReceived(rtype::test::peerHeader(9));
  const auto header = connection.makeHeader();
  CHECK_EQ(header.ack, 9u);
  CHECK_EQ(header.ackBits, 0x1u);
}

TEST(Connection, ReportsDuplicates) {
  Connection connection(1, localEndpoint(1));

  CHECK_EQ(connection.onPacketReceived(rtype::test::peerHeader(4)),
           PacketReceiveRes::New);
  CHECK_EQ(connection.onPacketReceived(rtype::test::peerHeader(4)),
           PacketReceiveRes::Dup);
}

TEST(Connection, SharesItsTracker) {
  Connection connection(1, localEndpoint(1));

  connection.getTracker().getNextSequence();
  CHECK_EQ(connection.makeHeader().sequence, 2u);
}

TEST(Connection, TouchUpdatesLastReceived) {
  Connection connection(1, localEndpoint(1));
  const auto when = rtype::test::at(1234);

  connection.touch(when);
  CHECK(connection.getLastReceived() == when);
}

TEST(Connection, LastReceivedStartsAtCreation) {
  const auto before = std::chrono::steady_clock::now();
  const Connection connection(1, localEndpoint(1));
  const auto after = std::chrono::steady_clock::now();

  CHECK(connection.getLastReceived() >= before);
  CHECK(connection.getLastReceived() <= after);
}

TEST(Connection, MetricsAreWritable) {
  Connection connection(1, localEndpoint(1));

  onPacketSend(connection.getMetrics(), 64);
  const Connection &view = connection;
  CHECK_EQ(view.getMetrics().packetsSent, 1u);
  CHECK_EQ(view.getMetrics().bytesSent, 64u);
}

TEST(Connection, EndpointsCompareByAddressAndPort) {
  const Connection first(1, localEndpoint(1000));
  const Connection second(2, localEndpoint(1001));

  CHECK(first.getEndpoint() != second.getEndpoint());
}
