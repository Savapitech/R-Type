#include "Server.hpp"

#include <iostream>
#include <mutex>
#include <utility>

#include "Protocol/EntityPayload.hpp"
#include "Protocol/MessageType.hpp"
#include "Protocol/PayloadCodec.hpp"
#include "Protocol/SnapshotPayload.hpp"
#include "Protocol/RoomPayload.hpp"
#include "config/NetworkConfig.hpp"
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
                  const std::vector<std::uint8_t> &payload,
                  PacketPriority priority) {
  _network.send(connectionId, payload, priority);
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

      if (_inputQueue.size() < MAX_QUEUED_EVENTS)
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
      if (_queryQueue.size() < MAX_QUEUED_EVENTS)
        _queryQueue.push({connectionId, query.networkId.get()});
    }
    break;
  }

  case protocol::MessageType::ListRooms: {
    if (!payload.empty())
      return;
    RoomRequestEvent event{};
    event.type = RoomRequestEvent::Type::ListRooms;
    event.connectionId = connectionId;
    {
      std::lock_guard<std::mutex> lock(_roomRequestMutex);
      if (_roomRequestQueue.size() < MAX_QUEUED_EVENTS)
        _roomRequestQueue.push(event);
    } 
    break;
  }

  case protocol::MessageType::CreateRoom: {
    PacketReader reader(payload);
    protocol::CreateRoomPayload req;
    if (!protocol::readCreateRoom(reader, req))
      return;
    RoomRequestEvent event{};
    event.type = RoomRequestEvent::Type::ListRooms;
    event.connectionId = connectionId;
    event.roomName = std::move(req.name);
    {
      std::lock_guard<std::mutex> lock(_roomRequestMutex);
      if (_roomRequestQueue.size() < MAX_QUEUED_EVENTS)
        _roomRequestQueue.push(event);
    } 
    break;
  }

  case protocol::MessageType::JoinRoom: {
    PacketReader reader(payload);
    protocol::JoinRoomPayload req{};
    if (!protocol::readJoinRoom(reader, req))
      return;
    RoomRequestEvent event{};
    event.type = RoomRequestEvent::Type::ListRooms;
    event.connectionId = connectionId;
    event.roomId = std::move(req.roomId);
    {
      std::lock_guard<std::mutex> lock(_roomRequestMutex);
      if (_roomRequestQueue.size() < MAX_QUEUED_EVENTS)
        _roomRequestQueue.push(event);
    } 
    break;
  }

  case protocol::MessageType::LeaveRoom: {
    if (!payload.empty())
      return;
    RoomRequestEvent event{};
    event.type = RoomRequestEvent::Type::LeaveRoom;
    event.connectionId = connectionId;
    {
      std::lock_guard<std::mutex> lock(_roomRequestMutex);
      if (_roomRequestQueue.size() < MAX_QUEUED_EVENTS)
        _roomRequestQueue.push(event);
    } 
    break;
  }

  case protocol::MessageType::SetReady: {
    PacketReader reader(payload);
    protocol::SetReadyPayload req{};
    if (!protocol::readSetReady(reader, req))
      return;
    RoomRequestEvent event{};
    event.type = RoomRequestEvent::Type::ListRooms;
    event.connectionId = connectionId;
    event.ready = std::move(req.ready);
    {
      std::lock_guard<std::mutex> lock(_roomRequestMutex);
      if (_roomRequestQueue.size() < MAX_QUEUED_EVENTS)
        _roomRequestQueue.push(event);
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

bool Server::pollRoomRequest(RoomRequestEvent &event) {
  std::lock_guard<std::mutex> lock(_roomRequestMutex);
  if (_roomRequestQueue.empty())
    return false;
  event = std::move(_roomRequestQueue.front());
  _roomRequestQueue.pop();
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

  send(connectionId, writer.getData(), protocol::messagePriority(type));
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

bool Server::sendRoomList(std::uint32_t connectionId, const protocol::RoomListPayload &payload) {
  PacketWriter writer;
  if (!protocol::writeRoomList(writer, payload))
    return false;
  return sendMessage(connectionId, protocol::MessageType::ListRooms, writer.getData());
}

bool Server::sendRoomCreated(std::uint32_t connectionId, const protocol::RoomResultPayload &payload) {
  PacketWriter writer;
  if (!protocol::writeRoomResult(writer, payload))
    return false;
  return sendMessage(connectionId, protocol::MessageType::RoomCreated, writer.getData());
}


bool Server::sendRoomJoined(std::uint32_t connectionId, const protocol::RoomResultPayload &payload) {
  PacketWriter writer;
  if (!protocol::writeRoomResult(writer, payload))
    return false;
  return sendMessage(connectionId, protocol::MessageType::RoomJoined, writer.getData());
}


bool Server::sendRoomLeft(std::uint32_t connectionId, const protocol::RoomResultPayload &payload) {
  PacketWriter writer;
  if (!protocol::writeRoomResult(writer, payload))
    return false;
  return sendMessage(connectionId, protocol::MessageType::RoomLeft, writer.getData());
}

bool Server::sendRoomStateUpdate(std::uint32_t connectionId, const protocol::RoomStateUpdatePayload &payload) {
  PacketWriter writer;
  if (!protocol::writeRoomStateUpdate(writer, payload))
    return false;
  return sendMessage(connectionId, protocol::MessageType::RoomStateUpdate, writer.getData());
}

bool Server::sendGameStarting(std::uint32_t connectionId, const protocol::GameStartingPayload &payload) {
  PacketWriter writer;
  if (!writer.writeUInt32(payload.roomId.get()))
    return false;
  return sendMessage(connectionId, protocol::MessageType::GameStarting, writer.getData());
}

} // namespace rtype::server
