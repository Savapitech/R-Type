#include "NetworkClient.hpp"
#include "Protocol/BigEndian.hpp"
#include "Protocol/EntityPayload.hpp"
#include "Protocol/InputPayload.hpp"
#include "Protocol/MessageType.hpp"
#include "Protocol/PayloadCodec.hpp"
#include "Protocol/SnapshotPayload.hpp"
#include "config/NetworkConfig.hpp"
#include "protocol/HandshakeHeader.hpp"
#include "protocol/MessageHeader.hpp"
#include "protocol/MessageParser.hpp"
#include "protocol/PacketHeader.hpp"
#include "protocol/PacketReader.hpp"
#include "protocol/SystemMessages.hpp"
#include "tracking/PacketTracker.hpp"

#include <asio/error.hpp>
#include <asio/error_code.hpp>
#include <asio/post.hpp>
#include <asio/registered_buffer.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>

namespace rtype::client {

NetworkClient::NetworkClient() : _socket(_ioContext), _flushTimer(_ioContext) {}

NetworkClient::~NetworkClient() { stop(); }

bool NetworkClient::connect(const std::string &host, const std::string &port) {

  if (_running.load() || _networkThread.joinable()) {
    return false;
  }

  try {
    asio::ip::udp::resolver resolver(_ioContext);

    const auto endpoints = resolver.resolve(asio::ip::udp::v4(), host, port);
    if (endpoints.empty())
      return false;

    _serverEndpoint = endpoints.begin()->endpoint();
    _socket.open(asio::ip::udp::v4());
    _running.store(true);

    startReceive();
    startFlushTimer();

    _networkThread = std::thread([this]() { _ioContext.run(); });
    return true;
  } catch (const std::exception &exception) {
    std::cerr << "[NetworkClient] Connect exception: " << exception.what()
              << '\n';

    stop();
    return false;
  }
}

void NetworkClient::sendHello() {
  if (!_running.load() || _connected.load())
    return;

  PacketWriter writer;

  HandshakeHeader hello;

  hello.magic = MAGIC;
  hello.version = VERSION;
  hello.type = HandshakeType::Hello;

  if (!hello.serialize(writer))
    return;
  send(writer.getData());
}

void NetworkClient::sendInput(std::uint32_t clientTick,
                              rtype::protocol::InputAction actions) {
  if (!_running.load() || !_connected.load())
    return;

  PacketWriter writer;

  MessageHeader messageHeader;
  messageHeader.setType(protocol::toMessageId(protocol::MessageType::Input));
  messageHeader.size =
      static_cast<std::uint16_t>(sizeof(rtype::protocol::InputPayload));

  if (!messageHeader.serialize(writer))
    return;

  asio::post(_ioContext,
             [this, clientTick, actions, writer = std::move(writer)]() mutable {
               if (!_running.load() || !_connected.load())
                 return;

               rtype::protocol::InputPayload input{
                   rtype::protocol::be32(_nextInputSequence),
                   rtype::protocol::be32(clientTick), actions};

               if (!rtype::protocol::writePayload(writer, input))
                 return;

               ++_nextInputSequence;

               enqueue(writer.getData(), PacketPriority::Unreliable);
             });
}

void NetworkClient::sendQueryEntity(std::uint32_t networkId) {
  if (!_running.load() || !_connected.load())
    return;

  PacketWriter writer;

  MessageHeader messageHeader;
  messageHeader.setType(
      protocol::toMessageId(protocol::MessageType::QueryEntity));
  messageHeader.size =
      static_cast<std::uint16_t>(sizeof(protocol::QueryEntityPayload));

  if (!messageHeader.serialize(writer))
    return;

  protocol::QueryEntityPayload payload;
  payload.networkId = networkId;
  if (!protocol::writePayload(writer, payload))
    return;

  asio::post(_ioContext, [this, data = writer.getData()]() mutable {
    if (!_running.load() || !_connected.load())
      return;
    enqueue(std::move(data), PacketPriority::Reliable);
  });
}

void NetworkClient::enqueue(std::vector<std::uint8_t> payload,
                            PacketPriority priority) {
  if (_tracker.enqueue(std::move(payload), priority))
    flush();
}

void NetworkClient::flush() {
  if (!_running.load() || !_connected.load())
    return;

  for (auto &datagram : _tracker.collectOutgoing(
           std::chrono::steady_clock::now(), _connectionId.load()))
    doSend(std::move(datagram));
}

void NetworkClient::startFlushTimer() {
  _flushTimer.expires_after(FLUSH_INTERVAL);
  _flushTimer.async_wait([this](const asio::error_code &error) {
    if (error || !_running.load())
      return;

    if (_connected.load() && _tracker.isTimedOut()) {
      _connected.store(false);
      pushEvent({.type = NetworkEventType::Disconnected,
                 .connectionId = _connectionId.load()});
    }
    flush();
    startFlushTimer();
  });
}

void NetworkClient::send(const std::vector<uint8_t> &data) {
  if (!_running.load())
    return;

  if (data.empty() || data.size() > MAX_PAYLOAD_SIZE)
    return;

  asio::post(_ioContext, [this, data]() {
    if (!_running.load())
      return;
    doSend(data);
  });
}

void NetworkClient::doSend(std::vector<uint8_t> data) {

  if (!_running.load() || !_socket.is_open()) {
    return;
  }

  auto buffer = std::make_shared<std::vector<std::uint8_t>>(std::move(data));

  _socket.async_send_to(
      asio::buffer(*buffer), _serverEndpoint,
      [buffer](const asio::error_code &error, std::size_t bytesSent) {
        if (error) {
          std::cerr << "[NetworkClient] UDP send failed: " << error.message()
                    << std::endl;
          return;
        }
      });
}
void NetworkClient::startReceive() {
  if (!_running.load() || !_socket.is_open())
    return;

  _socket.async_receive_from(
      asio::buffer(_receiveBuffer), _receiveEndpoint,
      [this](const asio::error_code &error, std::size_t bytesReceived) {
        if (!_running.load())
          return;

        if (!error) {
          if (_receiveEndpoint == _serverEndpoint && bytesReceived > 0 &&
              bytesReceived <= MAX_PAYLOAD_SIZE) {
            handleDatagram(bytesReceived);
          }
        }

        if (_running.load())
          startReceive();
      });
}

void NetworkClient::handleDatagram(std::size_t size) {
  if (!_connected.load()) {
    handleHandshake(_receiveBuffer.data(), size);
    return;
  }
  handleConnectedPackets(_receiveBuffer.data(), size);
}

void NetworkClient::handleHandshake(const std::uint8_t *data,
                                    std::size_t size) {
  std::vector<std::uint8_t> bytes(data, data + size);
  PacketReader reader(bytes);

  HandshakeHeader handshake;

  if (!handshake.deserialize(reader)) {
    return;
  }

  if (handshake.magic != MAGIC) {
    return;
  }

  if (handshake.version != VERSION) {
    return;
  }

  if (handshake.type == HandshakeType::Rejected) {
    if (!reader.isEnd())
      return;

    pushEvent({.type = NetworkEventType::Rejected});
    return;
  }

  if (handshake.type != HandshakeType::Welcome) {
    return;
  }

  std::uint32_t connectionId = 0;

  if (!reader.readUInt32(connectionId)) {
    return;
  }

  if (!reader.isEnd()) {
    return;
  }

  if (connectionId == 0) {
    return;
  }

  _connectionId.store(connectionId);
  _connected.store(true);

  pushEvent(
      {.type = NetworkEventType::Connected, .connectionId = connectionId});
}
void NetworkClient::handleConnectedPackets(const std::uint8_t *data,
                                           std::size_t size) {
  if (size < PacketHeader::HEADER_SIZE)
    return;
  std::vector<std::uint8_t> bytes(data, data + size);
  PacketReader reader(bytes);
  PacketHeader header;

  if (!header.deserialize(reader))
    return;

  if (header.connectionId != _connectionId.load())
    return;

  std::vector<ParsedMessage> messages;

  if (!MessageParser::parse(reader, messages))
    return;

  if (!_tracker.onPacketReceived(header, std::chrono::steady_clock::now()))
    return;

  for (auto &message : messages) {
    if (message.type == systemMessage::Disconnect) {
      _connected.store(false);
      pushEvent({.type = NetworkEventType::Disconnected,
                 .connectionId = header.connectionId});
      return;
    }

    const auto type = static_cast<protocol::MessageType>(message.type);

    switch (type) {
    case protocol::MessageType::CreateEntity:
    case protocol::MessageType::DestroyEntity:
    case protocol::MessageType::Snapshot:
      handleReplicationMessage(header.connectionId, type, message.payload);
      break;

    default:
      pushEvent({.type = NetworkEventType::Message,
                 .connectionId = header.connectionId,
                 .messageType = type,
                 .payload = std::move(message.payload)});
      break;
    }
  }
}

void NetworkClient::pushEvent(NetworkEvent event) {
  std::lock_guard<std::mutex> lock(_eventMutex);

  _events.push(std::move(event));
}

bool NetworkClient::pollEvent(NetworkEvent &event) {
  std::lock_guard<std::mutex> lock(_eventMutex);

  if (_events.empty())
    return false;

  event = std::move(_events.front());
  _events.pop();

  return true;
}

bool NetworkClient::isConnected() const { return _connected.load(); }

bool NetworkClient::isRunning() const { return _running.load(); }

std::uint32_t NetworkClient::getConnectionId() const {
  return _connectionId.load();
}

void NetworkClient::stop() {
  if (!_running.exchange(false)) {
    if (_networkThread.joinable())
      _networkThread.join();

    return;
  }

  asio::post(_ioContext, [this]() {
    asio::error_code ignored;
    _flushTimer.cancel();
    _socket.cancel(ignored);
    _socket.close(ignored);
  });

  if (_networkThread.joinable()) {
    _networkThread.join();
  } else {
    asio::error_code ignored;
    _socket.close(ignored);
  }

  _connected.store(false);
  _connectionId.store(0);
}

bool NetworkClient::pollReplicationEvent(ReplicationEvent &event) {
  std::lock_guard<std::mutex> lock(_eventMutex);

  if (_replicationEvents.empty())
    return false;

  event = std::move(_replicationEvents.front());
  _replicationEvents.pop();

  return true;
}

void NetworkClient::handleReplicationMessage(
    std::uint32_t connectionId, protocol::MessageType type,
    const std::vector<std::uint8_t> &payload) {
  PacketReader reader(payload);

  switch (type) {
  case protocol::MessageType::CreateEntity: {
    protocol::CreateEntityPayload decoded;

    if (!protocol::readPayload(reader, decoded) || !reader.isEnd() ||
        !protocol::isValid(decoded))
      return;

    std::lock_guard<std::mutex> lock(_eventMutex);
    _replicationEvents.push({connectionId, std::move(decoded)});
    return;
  }
  case protocol::MessageType::DestroyEntity: {
    protocol::DestroyEntityPayload decoded;

    if (!protocol::readPayload(reader, decoded) || !reader.isEnd() ||
        !protocol::isValid(decoded))
      return;

    std::lock_guard<std::mutex> lock(_eventMutex);
    _replicationEvents.push({connectionId, std::move(decoded)});
    return;
  }
  case protocol::MessageType::Snapshot: {
    protocol::SnapshotPayload decoded;

    if (!protocol::readSnapshot(reader, decoded) || !reader.isEnd())
      return;

    std::lock_guard<std::mutex> lock(_eventMutex);
    _replicationEvents.push({connectionId, std::move(decoded)});
    return;
  }

  default:
    return;
  }
}

} // namespace rtype::client
