#include "Server.hpp"

#include <iostream>
#include <utility>

#include "Protocol/EntityPayload.hpp"
#include "Protocol/MessageType.hpp"
#include "Protocol/PayloadCodec.hpp"
#include "Protocol/SnapshotPayload.hpp"
#include "protocol/MessageHeader.hpp"
#include "protocol/PacketReader.hpp"
#include "protocol/PacketWriter.hpp"

namespace rtype::server {

static bool isSequenceNewer(std::uint32_t sequence, std::uint32_t previous) {
  return static_cast<std::int32_t>(sequence - previous) > 0;
}

Server::Server(std::uint16_t port) : _network(port) {
  _network.setOnClientConnected([this](std::uint32_t connectionId) {
    std::cout << "Client connected: " << connectionId << std::endl;
    {
      std::lock_guard<std::mutex> lock(_connectionMutex);
      _connectionQueue.push({ConnectionEvent::Type::Connected, connectionId});
    }
  });

  _network.setOnClientDisconnected([this](std::uint32_t connectionId) {
    _lastInputSequence.erase(connectionId);
    std::cout << "Client disconnected: " << connectionId << std::endl;
    {
      std::lock_guard<std::mutex> lock(_connectionMutex);
      _connectionQueue.push(
          {ConnectionEvent::Type::Disconnected, connectionId});
    }
  });

  _network.setOnMessage([this](std::uint32_t connectionId, MessageId type,
                               const std::vector<std::uint8_t> &payload) {
    handleMessage(connectionId, type, payload);
  });
}

Server::~Server() { stop(); }

void Server::start() { _network.start(); }

void Server::stop() { _network.stop(); }

void Server::send(std::uint32_t connectionId,
                  const std::vector<std::uint8_t> &payload) {
  _network.send(connectionId, payload);
}

void Server::handleMessage(std::uint32_t connectionId, MessageId type,
                           const std::vector<std::uint8_t> &payload) {
  switch (static_cast<protocol::MessageType>(type)) {
  case protocol::MessageType::Input: {
    PacketReader reader(payload);
    protocol::InputPayload input;
    if (!protocol::readPayload(reader, input) || !reader.isEnd()) {
      return;
    }
    const std::uint32_t sequence = input.inputSequence.get();
    auto it = _lastInputSequence.find(connectionId);

    if (it != _lastInputSequence.end()) {
      if (!isSequenceNewer(sequence, it->second)) {
        return;
      }
      it->second = sequence;
    } else {
      _lastInputSequence.emplace(connectionId, sequence);
    }

    {
      std::lock_guard<std::mutex> lock(_inputMutex);

      _inputQueue.push({connectionId, input});
    }
    break;
  }

  case protocol::MessageType::QueryEntity: {
    PacketReader reader(payload);
    protocol::QueryEntityPayload query;
    if (!protocol::readPayload(reader, query) || !reader.isEnd() ||
        !protocol::isValid(query)) {
      return;
    }
    {
      std::lock_guard<std::mutex> lock(_queryMutex);
      _queryQueue.push({connectionId, query.networkId.get()});
    }
    break;
  }

  default:
    break;
  }
}

bool Server::pollInput(InputEvent &event) {
  std::lock_guard<std::mutex> lock(_inputMutex);

  if (_inputQueue.empty())
    return false;

  event = _inputQueue.front();
  _inputQueue.pop();
  return true;
}

bool Server::pollConnectionEvent(ConnectionEvent &event) {
  std::lock_guard<std::mutex> lock(_connectionMutex);

  if (_connectionQueue.empty())
    return false;

  event = _connectionQueue.front();
  _connectionQueue.pop();
  return true;
}

bool Server::pollQueryEntity(QueryEntityEvent &event) {
  std::lock_guard<std::mutex> lock(_queryMutex);

  if (_queryQueue.empty())
    return false;

  event = _queryQueue.front();
  _queryQueue.pop();
  return true;
}

bool Server::sendMessage(std::uint32_t connectionId, protocol::MessageType type,
                         const std::vector<std::uint8_t> &payload) {
  if (connectionId == 0 ||
      payload.size() > MAX_MESSAGE_SIZE - MessageHeader::HEADER_SIZE ||
      payload.size() > std::numeric_limits<std::uint16_t>::max())
    return false;

  PacketWriter writer;
  MessageHeader header;

  header.setType(protocol::toMessageId(type));
  header.size = static_cast<std::uint16_t>(payload.size());
  if (!header.serialize(writer))
    return false;

  if (!writer.writeBytes(payload.data(), payload.size()))
    return false;

  send(connectionId, writer.getData());
  return true;
}

bool Server::sendCreateEntity(std::uint32_t connectionId,
                              const protocol::CreateEntityPayload &payload) {
  PacketWriter writer;

  if (!protocol::isValid(payload) || !protocol::writePayload(writer, payload))
    return false;

  return sendMessage(connectionId, protocol::MessageType::CreateEntity,
                     writer.getData());
}

bool Server::sendDestroyEntity(std::uint32_t connectionId,
                               const protocol::DestroyEntityPayload &payload) {
  PacketWriter writer;

  if (!protocol::isValid(payload) || !protocol::writePayload(writer, payload))
    return false;

  return sendMessage(connectionId, protocol::MessageType::DestroyEntity,
                     writer.getData());
}

bool Server::sendSnapshot(std::uint32_t connectionId,
                          const protocol::SnapshotPayload &payload) {
  PacketWriter writer;

  if (!protocol::writeSnapshot(writer, payload))
    return false;

  return sendMessage(connectionId, protocol::MessageType::Snapshot,
                     writer.getData());
}
} // namespace rtype::server
