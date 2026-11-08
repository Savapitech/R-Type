#include "NetworkClient.hpp"
#include "Protocol/BigEndian.hpp"
#include "Protocol/ChatPayload.hpp"
#include "Protocol/DisconnectPayload.hpp"
#include "Protocol/EntityPayload.hpp"
#include "Protocol/InputPayload.hpp"
#include "Protocol/MessageType.hpp"
#include "Protocol/PayloadCodec.hpp"
#include "Protocol/RoomPayload.hpp"
#include "Protocol/SnapshotPayload.hpp"
#include "config/NetworkConfig.hpp"
#include "metrics/Metrics.hpp"
#include "protocol/HandshakeHeader.hpp"
#include "protocol/MessageHeader.hpp"
#include "protocol/MessageParser.hpp"
#include "protocol/PacketHeader.hpp"
#include "protocol/PacketReader.hpp"
#include "protocol/PacketWriter.hpp"
#include "protocol/PingMessage.hpp"
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
#include <utility>

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

  asio::post(_ioContext, [this, clientTick, actions]() {
    if (!_running.load() || !_connected.load())
      return;

    const auto now = std::chrono::steady_clock::now();
    if (!_hasSentInput || actions != _lastSentActions) {
      _redundantInputsLeft = INPUT_REDUNDANT_COPIES;
    } else if (_redundantInputsLeft > 0) {
      --_redundantInputsLeft;
    } else if (now - _lastInputSent < INPUT_HEARTBEAT) {
      return;
    }
    _hasSentInput = true;
    _lastSentActions = actions;
    _lastInputSent = now;

    PacketWriter writer;
    MessageHeader messageHeader;
    messageHeader.setType(protocol::toMessageId(protocol::MessageType::Input));
    messageHeader.size =
        static_cast<std::uint16_t>(sizeof(rtype::protocol::InputPayload));
    if (!messageHeader.serialize(writer))
      return;

    rtype::protocol::InputPayload input{
        rtype::protocol::be32(_nextInputSequence),
        rtype::protocol::be32(clientTick), actions};
    if (!rtype::protocol::writePayload(writer, input))
      return;

    ++_nextInputSequence;
    enqueue(writer.getData(),
            protocol::messagePriority(protocol::MessageType::Input));
  });
}

std::uint32_t NetworkClient::inputTick() const {
  const double elapsed = std::chrono::duration<double>(
                             std::chrono::steady_clock::now() - _inputEpoch)
                             .count();
  return static_cast<std::uint32_t>(elapsed * INPUT_TICK_RATE);
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
    enqueue(std::move(data),
            protocol::messagePriority(protocol::MessageType::QueryEntity));
  });
}

void NetworkClient::enqueue(std::vector<std::uint8_t> payload,
                            PacketPriority priority) {
  if (_tracker.enqueue(std::move(payload), priority))
    flush();
}

void NetworkClient::sendRoomCommand(protocol::MessageType type,
                                    const std::vector<std::uint8_t> &payload) {
  if (!_running.load() || !_connected.load()) {
    return;
  }
  if (payload.size() > MAX_MESSAGE_SIZE - MessageHeader::HEADER_SIZE) {
    return;
  }
  PacketWriter writer;
  MessageHeader header;
  header.setType(protocol::toMessageId(type));
  header.size = static_cast<std::uint16_t>(payload.size());

  if (!header.serialize(writer)) {
    return;
  }
  if (!payload.empty()) {
    if (!writer.writeBytes(payload.data(), payload.size()))
      return;
  }
  asio::post(_ioContext, [this, type, data = writer.getData()]() mutable {
    if (!_running.load() || !_connected.load()) {
      return;
    }
    enqueue(std::move(data), protocol::messagePriority(type));
  });
}

void NetworkClient::sendListRoom() {
  sendRoomCommand(protocol::MessageType::ListRooms, {});
}

void NetworkClient::sendCreateRoom(const std::string &name) {
  if (name.empty() || name.size() > protocol::MAX_ROOM_NAME_LENGTH) {
    return;
  }

  PacketWriter writer;
  protocol::CreateRoomPayload payload{};
  payload.name = name;

  if (!protocol::writeCreateRoom(writer, payload)) {
    return;
  }
  sendRoomCommand(protocol::MessageType::CreateRoom, writer.getData());
}

void NetworkClient::sendJoinRoom(std::uint32_t roomId) {
  PacketWriter writer;

  if (!writer.writeUInt32(roomId)) {
    return;
  }
  sendRoomCommand(protocol::MessageType::JoinRoom, writer.getData());
}

void NetworkClient::sendLeaveRoom() {
  sendRoomCommand(protocol::MessageType::LeaveRoom, {});
}

void NetworkClient::sendSetReady(bool ready) {
  PacketWriter writer;
  std::uint8_t status = 0U;

  if (ready == true) {
    status = 1U;
  }
  if (!writer.writeUInt8(status)) {
    return;
  }
  sendRoomCommand(protocol::MessageType::SetReady, writer.getData());
}

void NetworkClient::sendChat(const std::string &text) {
  PacketWriter writer;
  protocol::ChatSendPayload payload{};
  payload.text = text;

  if (!protocol::writeChatSend(writer, payload)) {
    return;
  }
  sendRoomCommand(protocol::MessageType::ChatSend, writer.getData());
}

void NetworkClient::sendSetUsername(const std::string &username) {
  sendRoomCommand(protocol::MessageType::SetUsername,
                  std::vector<std::uint8_t>(username.begin(), username.end()));
}

void NetworkClient::sendGetScoreboard() {
  sendRoomCommand(protocol::MessageType::GetScoreboard, {});
}

void NetworkClient::flush() {
  if (!_running.load() || !_connected.load())
    return;

  const auto previousResends = _tracker.getResendCount();
  auto datagrams = _tracker.collectOutgoing(std::chrono::steady_clock::now(),
                                            _connectionId.load());
  const auto newResends = _tracker.getResendCount() - previousResends;
  for (std::uint32_t i = 0; i < newResends; i++)
    onRetransmission(_metrics);

  for (auto &datagram : datagrams)
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
    sendKeepalive();
    flush();
    {
      std::lock_guard<std::mutex> lock(_metricsMutex);
      _metricsSnapshot = _metrics;
    }
    startFlushTimer();
  });
}

void NetworkClient::sendKeepalive() {
  if (!_connected.load())
    return;

  const auto now = std::chrono::steady_clock::now();
  if (now - _lastKeepalive < KEEPALIVE_INTERVAL)
    return;
  _lastKeepalive = now;

  PacketWriter writer;
  MessageHeader header;
  header.setType(systemMessage::Ping);
  header.size = PingMessage::SIZE;

  PingMessage ping;
  ping.timestamp = static_cast<std::uint32_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          now.time_since_epoch())
          .count());

  if (header.serialize(writer) && ping.serialize(writer))
    _tracker.enqueue(writer.getData(), PacketPriority::Unreliable);
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
      [this, buffer](const asio::error_code &error, std::size_t bytesSent) {
        if (error) {
          std::cerr << "[NetworkClient] UDP send failed: " << error.message()
                    << std::endl;
          return;
        }
        onPacketSend(_metrics, bytesSent);
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
            onPacketReceived(_metrics, bytesReceived);
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
  _hasSentInput = false;
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

  const auto previousDup = _tracker.getDuplicateCount();
  const auto previousOut = _tracker.getOutOfOrderCount();
  const auto previousRttSamples = _tracker.getRttSampleCount();
  const bool accepted =
      _tracker.onPacketReceived(header, std::chrono::steady_clock::now());

  const auto newDup = _tracker.getDuplicateCount() - previousDup;
  for (std::uint64_t i = 0; i < newDup; i++) {
    onPacketDuplicated(_metrics);
  }

  const auto newOut = _tracker.getOutOfOrderCount() - previousOut;
  for (std::uint64_t i = 0; i < newOut; i++) {
    onPacketOut(_metrics);
  }

  if (_tracker.getRttSampleCount() > previousRttSamples) {
    const float rttMs =
        static_cast<float>(_tracker.getLastRttSample().count()) / 1000.0f;
    updateRttMetrics(_metrics, rttMs);
  }

  if (!accepted)
    return;

  std::vector<ParsedMessage> messages;
  if (!MessageParser::parseBody(header.compressed, reader, messages))
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

  if (_events.size() < MAX_QUEUED_EVENTS)
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

    if (_connected.load() && _socket.is_open()) {
      PacketWriter writer;
      MessageHeader header;
      header.setType(systemMessage::Disconnect);
      header.size = sizeof(protocol::DisconnectPayload);

      if (header.serialize(writer) &&
          writer.writeUInt8(static_cast<std::uint8_t>(
              protocol::DisconnectReason::ClientQuit)) &&
          _tracker.enqueue(writer.getData(), PacketPriority::Unreliable)) {
        for (const auto &datagram : _tracker.collectOutgoing(
                 std::chrono::steady_clock::now(), _connectionId.load()))
          _socket.send_to(asio::buffer(datagram), _serverEndpoint, 0, ignored);
      }
    }

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
    if (_replicationEvents.size() < MAX_QUEUED_EVENTS)
      _replicationEvents.push({connectionId, std::move(decoded)});
    return;
  }
  case protocol::MessageType::DestroyEntity: {
    protocol::DestroyEntityPayload decoded;

    if (!protocol::readPayload(reader, decoded) || !reader.isEnd() ||
        !protocol::isValid(decoded))
      return;

    std::lock_guard<std::mutex> lock(_eventMutex);
    if (_replicationEvents.size() < MAX_QUEUED_EVENTS)
      _replicationEvents.push({connectionId, std::move(decoded)});
    return;
  }
  case protocol::MessageType::Snapshot: {
    protocol::SnapshotPayload decoded;

    if (!protocol::readSnapshot(reader, decoded) || !reader.isEnd())
      return;

    std::lock_guard<std::mutex> lock(_eventMutex);
    if (_replicationEvents.size() < MAX_QUEUED_EVENTS)
      _replicationEvents.push({connectionId, std::move(decoded)});
    return;
  }

  default:
    return;
  }
}

NetworkMetrics NetworkClient::getMetrics() const {
  std::lock_guard<std::mutex> lock(_metricsMutex);
  return _metricsSnapshot;
}

} // namespace rtype::client
