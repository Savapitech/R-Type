#include "Connection.hpp"

Connection::Connection(std::uint32_t id, const asio::ip::udp::endpoint &endpoint) :
  _id(id), _endpoint(endpoint)
{}

std::uint32_t Connection::getId() const {
  return _id;
}

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
