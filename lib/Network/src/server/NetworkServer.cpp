#include "NetworkServer.hpp"
#include "../connection/Connection.hpp"
#include "../protocol/HandshakeHeader.hpp"
#include "../protocol/PacketWriter.hpp"
#include "../protocol/PingMessage.hpp"
#include "../protocol/PongMessage.hpp"
#include "../protocol/SystemMessages.hpp"

#include <asio/post.hpp>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <memory>
#include <ostream>
#include <thread>
#include <vector>

NetworkServer::NetworkServer(std::uint16_t port)
    : _io(), _metrics(), _transport(_io, port, _metrics) {}

NetworkServer::~NetworkServer() { stop(); }

void NetworkServer::start() {
  if (_running.exchange(true))
    return;

  _transport.setReceiveCallback([this](const asio::ip::udp::endpoint &endpoint,
                                       const std::vector<std::uint8_t> &data) {
    handleDatagram(endpoint, data);
  });

  _transport.startReceive();

  _networkThread = std::thread([this]() { _io.run(); });
}

void NetworkServer::stop() {
  if (!_running.exchange(false))
    return;

  _transport.close();

  if (_networkThread.joinable())
    _networkThread.join();
}

Connection *NetworkServer::findConnectionByEndpoint(
    const asio::ip::udp::endpoint &endpoint) {
  for (auto &[_, connection] : _connections) {
    if (connection->getEndpoint() == endpoint)
      return connection.get();
  }
  return nullptr;
}

void NetworkServer::handleHandshake(const asio::ip::udp::endpoint &endpoint,
                                    const std::vector<std::uint8_t> &data) {
  PacketReader reader(data);

  HandshakeHeader handshake;

  if (!handshake.deserialize(reader))
    return;

  if (handshake.version != VERSION)
    return;

  if (handshake.type != HandshakeType::Hello)
    return;

  std::uint32_t connectionId = _nextId++;
  auto connection = std::make_unique<Connection>(connectionId, endpoint);

  auto [it, inserted] =
      _connections.emplace(connectionId, std::move(connection));
  if (!inserted)
    return;

  sendWelcome(*it->second);

  if (_onClientConnected)
    _onClientConnected(connectionId);
}

void NetworkServer::handleConnectedPacket(
    const asio::ip::udp::endpoint &endpoint,
    const std::vector<std::uint8_t> &data) {
  if (data.size() < PacketHeader::HEADER_SIZE)
    return;

  PacketReader reader(data);
  PacketHeader header;

  if (!header.deserialize(reader))
    return;

  auto it = _connections.find(header.connectionId);
  if (it == _connections.end())
    return;

  Connection &connection = *it->second;
  if (connection.getEndpoint() != endpoint)
    return;

  PacketReceiveRes res = connection.onPacketReceived(header);

  if (res == PacketReceiveRes::TooOld || res == PacketReceiveRes::Dup)
    return;

  std::vector<ParsedMessage> messages;
  if (!MessageParser::parse(reader, messages)) {
    std::cerr << "Invalid message framing" << header.connectionId << std::endl;
    return;
  }

  for (const auto &message : messages) {
    handleMessage(header.connectionId, message);
  }
}

void NetworkServer::handleDatagram(const asio::ip::udp::endpoint &endpoint,
                                   const std::vector<std::uint8_t> &data) {
  Connection *connection = findConnectionByEndpoint(endpoint);

  if (connection == nullptr) {
    handleHandshake(endpoint, data);
    return;
  }
  handleConnectedPacket(endpoint, data);
}

void NetworkServer::sendWelcome(Connection &connection) {
  PacketWriter writer;

  HandshakeHeader header;
  header.magic = MAGIC;
  header.type = HandshakeType::Welcome;
  header.version = VERSION;

  if (!header.serialize(writer))
    return;

  if (!writer.writeUInt32(connection.getId()))
    return;

  _transport.send(connection.getEndpoint(), writer.getData());
}

void NetworkServer::doSend(std::uint32_t connectionId,
                           const std::vector<std::uint8_t> &payload) {
  auto it = _connections.find(connectionId);
  if (it == _connections.end())
    return;

  Connection &connection = *it->second;

  if (payload.size() > MAX_MESSAGE_SIZE)
    return;

  PacketWriter writer;
  PacketHeader header = connection.makeHeader();
  if (!header.serialize(writer))
    return;

  if (!writer.writeBytes(payload.data(), payload.size()))
    return;

  _transport.send(connection.getEndpoint(), writer.getData());
}

void NetworkServer::send(std::uint32_t connectionId,
                         const std::vector<std::uint8_t> &payload) {
  if (!_running.load())
    return;
  asio::post(
      _io, [this, connectionId, payload]() { doSend(connectionId, payload); });
}

void NetworkServer::disconnectClient(std::uint32_t connectionId) {
  if (_connections.erase(connectionId) == 0)
    return;

  if (_onClientDisconnected)
    _onClientDisconnected(connectionId);
}

void NetworkServer::handleMessage(std::uint32_t connectionId,
                                  const ParsedMessage &message) {
  switch (message.type) {
  case systemMessage::Ping: {
    PacketReader reader(message.payload);
    PingMessage ping;

    if (!ping.deserialize(reader) || !reader.isEnd())
      return;

    PacketWriter writer;
    MessageHeader header;
    header.setType(systemMessage::Pong);
    header.size = PongMessage::SIZE;

    PongMessage pong;
    pong.timestamp = ping.timestamp;

    if (!header.serialize(writer) || !pong.serialize(writer))
      return;

    doSend(connectionId, writer.getData());
    break;
  }

  case systemMessage::Disconnect:
    disconnectClient(connectionId);
    break;

  default:
    if (_onMessage)
      _onMessage(connectionId, message.type, message.payload);
    break;
  }
}

void NetworkServer::setOnClientConnected(ConnectionCallback callback) {
  _onClientConnected = std::move(callback);
}

void NetworkServer::setOnClientDisconnected(ConnectionCallback callback) {
  _onClientDisconnected = std::move(callback);
}

void NetworkServer::setOnMessage(MessageCallback callback) {
  _onMessage = std::move(callback);
}
