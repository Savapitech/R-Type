#include "Connection.hpp"
#include "metrics/Metrics.hpp"

Connection::Connection(std::uint32_t id,
                       const asio::ip::udp::endpoint &endpoint)
    : _id(id), _endpoint(endpoint),
      _lastReceived(std::chrono::steady_clock::now()) {}

std::uint32_t Connection::getId() const { return _id; }

const asio::ip::udp::endpoint &Connection::getEndpoint() const {
  return _endpoint;
}

PacketHeader Connection::makeHeader() {
  PacketHeader header;

  header.connectionId = _id;
  header.sequence = _tracker.getNextSequence();
  header.ack = _tracker.getAck();
  header.ackBits = _tracker.getAckBits();

  return header;
}

PacketReceiveRes Connection::onPacketReceived(const PacketHeader &header) {
  return _tracker.onPacketReceived(header.sequence);
}

PacketTracker &Connection::getTracker() { return _tracker; }

void Connection::touch(std::chrono::steady_clock::time_point now) {
  _lastReceived = now;
}

std::chrono::steady_clock::time_point Connection::getLastReceived() const {
  return _lastReceived;
}

void Connection::recordRttSample(std::chrono::microseconds sample,
                                 std::chrono::steady_clock::time_point now) {
  constexpr auto WINDOW = std::chrono::milliseconds(250);
  if (now - _rttWindowStart >= WINDOW) {
    if (_rttWindowMin > 0.0f)
      _metrics.windowMinRtt = _rttWindowMin;
    _rttWindowStart = now;
    _rttWindowMin = 0.0f;
  }
  const float ms = static_cast<float>(sample.count()) / 1000.0f;
  if (_rttWindowMin == 0.0f || ms < _rttWindowMin)
    _rttWindowMin = ms > 0.0f ? ms : 0.001f;
}

NetworkMetrics &Connection::getMetrics() { return _metrics; }

const NetworkMetrics &Connection::getMetrics() const { return _metrics; }
