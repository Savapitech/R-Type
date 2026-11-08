#include "game.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <variant>
#include <vector>

#include "GameInstance/RoomChannels.hpp"
#include "GameInstance/RoomWorker.hpp"
#include "Protocol/ChatPayload.hpp"
#include "Protocol/RoomPayload.hpp"
#include "Room/Room.hpp"
#include "Room/RoomManager.hpp"

#include "EngineContext.hpp"
#include "Server.hpp"

namespace rtype::server {

Game::Game(std::uint16_t port) : _server(port) {
  _server.setInputHandler([this](const Server::InputEvent &event) {
    if (auto mailbox = _roomRouter.find(event.connectionId))
      mailbox->postInput(event);
  });
  _server.setQueryEntityHandler([this](const Server::QueryEntityEvent &event) {
    if (auto mailbox = _roomRouter.find(event.connectionId))
      mailbox->postQuery(event);
  });
}

Game::~Game() = default;

void Game::onLoad(rtk::EngineContext &) {
  try {
    std::ifstream file("scores.json");
    if (file)
      _stats = nlohmann::json::parse(file);
  } catch (const std::exception &error) {
    std::cerr << "[Server] Cannot read scores.json: " << error.what()
              << std::endl;
  }
  if (!_stats.is_object())
    _stats = nlohmann::json::object();

  _server.start();
  _defaultRoomId = _roomManager.createRoom("Default Room", true);
  RoomWorker *worker = createRoomWorker(_defaultRoomId);

  if (!worker)
    throw std::runtime_error("Failed to create default game Instance.");

  worker->waitUntilStarted();
  std::cout << "[Server] Game simulation started" << std::endl;
}

void Game::handleConnections(rtk::EngineContext &) {

  Server::ConnectionEvent event;
  while (_server.pollConnectionEvent(event)) {
    const auto connectionId = event.connectionId;
    if (event.type == Server::ConnectionEvent::Type::Connected) {
      std::cout << "[Server] Client " << connectionId
                << " connected. Waiting for room selection." << std::endl;
    } else if (event.type == Server::ConnectionEvent::Type::Disconnected) {
      const auto roomId = _roomManager.getRoomId(connectionId);
      const std::string name = displayName(connectionId);
      if (removePlayerFromRoom(connectionId, false) && roomId)
        broadcastRoomNotice(*roomId, name + " LOST CONNECTION");
      _lastChatMessage.erase(connectionId);
      _usernames.erase(connectionId);
      std::cout << "[Server] Client " << connectionId << " disconnected"
                << std::endl;
    }
  }
}

void Game::onUpdate(rtk::EngineContext &context, float) {
  handleConnections(context);
  handleRoomRequests();
  handleRoomEvents();
  updateNetworkMetrics();
}
bool Game::shouldContinue() const { return _running; }

void Game::onUnload(rtk::EngineContext &) {
  for (auto &[roomId, worker] : _roomWorkers)
    worker->requestStop();

  _roomWorkers.clear();
  _server.stop();
  std::cout << "[Server] Multiples instances stopped" << std::endl;
}

RoomWorker *Game::createRoomWorker(std::uint32_t roomId) {
  if (_roomManager.getRoom(roomId) == nullptr)
    return nullptr;

  auto it = _roomWorkers.find(roomId);
  if (it != _roomWorkers.end())
    return it->second.get();

  try {
    auto worker = std::make_unique<RoomWorker>(roomId, _server, _roomEvents);
    RoomWorker *ptr = worker.get();
    _roomWorkers.emplace(roomId, std::move(worker));
    return ptr;
  } catch (const std::system_error &error) {
    std::cerr << "[Server] Failed to start room " << roomId << ": "
              << error.what() << std::endl;
    return nullptr;
  }
}

RoomWorker *Game::getRoomWorker(std::uint32_t roomId) {
  auto it = _roomWorkers.find(roomId);
  if (it == _roomWorkers.end())
    return nullptr;

  return it->second.get();
}

bool Game::destroyRoomWorker(std::uint32_t roomId) {
  return _roomWorkers.erase(roomId) > 0;
}

void Game::beginJoin(std::uint32_t connectionId, std::uint32_t roomId,
                     RoomWorker &worker, JoinKind kind) {
  const std::uint64_t requestId = _nextJoinRequestId++;

  _pendingJoins.insert_or_assign(connectionId,
                                 PendingJoin{roomId, requestId, kind});
  _roomRouter.bind(connectionId, worker.mailbox());
  worker.mailbox()->post(AddPlayerCommand{connectionId, requestId});
}

void Game::sendJoinResult(std::uint32_t connectionId, JoinKind kind,
                          const protocol::RoomResultPayload &result) {
  if (kind == JoinKind::Create)
    _server.sendRoomCreated(connectionId, result);
  else
    _server.sendRoomJoined(connectionId, result);
}

bool Game::removePlayerFromRoom(std::uint32_t connectionId, bool notifyClient) {
  auto roomId = _roomManager.getRoomId(connectionId);
  if (!roomId)
    return false;
  const std::uint32_t id = *roomId;

  _pendingJoins.erase(connectionId);
  _roomRouter.unbind(connectionId);
  if (auto *worker = getRoomWorker(id))
    worker->mailbox()->post(RemovePlayerCommand{connectionId, notifyClient});
  _roomManager.leaveRoom(connectionId);
  broadcastRoomState(id);
  std::cout << "[Server] Player " << connectionId << " left room " << id
            << std::endl;
  startRoomIfReady(id);
  return true;
}

void Game::freeRoomListSlot() {
  while (_roomManager.listRooms().size() >= protocol::MAX_ROOMS_PER_LIST) {
    const auto roomId = _roomManager.deleteOldestEmptyRoom();
    if (!roomId)
      return;
    destroyRoomWorker(*roomId);
    std::cout << "[Server] Removed empty room " << *roomId << std::endl;
  }
}

void Game::startRoomIfReady(std::uint32_t roomId) {
  auto *room = _roomManager.getRoom(roomId);
  auto *worker = getRoomWorker(roomId);

  if (!room || !worker || room->getState() != RoomState::Waiting ||
      !_roomManager.areAllPlayersReady(roomId))
    return;

  room->setState(RoomState::Playing);
  protocol::GameStartingPayload payload{};
  payload.roomId = roomId;
  for (const auto connectionId : room->getPlayers())
    _server.sendGameStarting(connectionId, payload);
  worker->mailbox()->post(StartGameCommand{});
  std::cout << "[Room " << roomId << "] All players ready. Starting game!"
            << std::endl;
}

void Game::handleRoomEvents() {
  RoomEvent event{};

  while (_roomEvents.poll(event)) {
    switch (event.type) {
    case RoomEvent::Type::PlayerAdded:
      finishJoin(event);
      break;
    case RoomEvent::Type::GameEnded:
      saveScores(event.scores);
      finishGame(event.roomId);
      break;
    case RoomEvent::Type::Crashed:
      handleRoomCrash(event);
      break;
    }
  }
}

void Game::finishJoin(const RoomEvent &event) {
  const auto it = _pendingJoins.find(event.connectionId);
  if (it == _pendingJoins.end() || it->second.requestId != event.requestId)
    return;

  const auto [roomId, requestId, kind] = it->second;
  _pendingJoins.erase(it);

  protocol::RoomResultPayload result{};
  result.roomId = roomId;
  if (event.success) {
    result.result = protocol::RoomResult::Success;
    sendJoinResult(event.connectionId, kind, result);
    broadcastRoomState(roomId);
    std::cout << "[Server] Player " << event.connectionId
              << (kind == JoinKind::Create ? " created room " : " joined room ")
              << roomId << std::endl;
    return;
  }

  _roomRouter.unbind(event.connectionId);
  _roomManager.leaveRoom(event.connectionId);
  if (kind == JoinKind::Create && _roomManager.deleteRoom(roomId))
    destroyRoomWorker(roomId);
  result.result = protocol::RoomResult::InternalError;
  sendJoinResult(event.connectionId, kind, result);
  broadcastRoomState(roomId);
  startRoomIfReady(roomId);
}

void Game::finishGame(std::uint32_t roomId) {
  auto *room = _roomManager.getRoom(roomId);
  if (!room)
    return;

  room->setState(RoomState::Waiting);
  _roomManager.resetReadyStates(roomId);
  broadcastRoomState(roomId);
}

void Game::handleRoomCrash(const RoomEvent &event) {
  const std::uint32_t roomId = event.roomId;
  std::cerr << "[Server] Room " << roomId << " crashed: " << event.error
            << std::endl;

  // Requests posted after the crash will never be answered by the room.
  RoomMailbox::Batch unanswered;
  if (auto *worker = getRoomWorker(roomId)) {
    worker->stop();
    worker->mailbox()->drain(unanswered);
  }
  protocol::RoomResultPayload left{};
  left.result = protocol::RoomResult::Success;
  left.roomId = roomId;
  for (const auto &command : unanswered.commands) {
    const auto *remove = std::get_if<RemovePlayerCommand>(&command);
    if (remove && remove->notifyClient)
      _server.sendRoomLeft(remove->connectionId, left);
  }

  auto *room = _roomManager.getRoom(roomId);
  if (room) {
    const std::vector<std::uint32_t> players = room->getPlayers();
    for (const auto connectionId : players) {
      _roomRouter.unbind(connectionId);
      _roomManager.leaveRoom(connectionId);

      const auto pending = _pendingJoins.find(connectionId);
      if (pending == _pendingJoins.end()) {
        _server.sendRoomLeft(connectionId, left);
        continue;
      }
      protocol::RoomResultPayload failure{};
      failure.result = protocol::RoomResult::InternalError;
      failure.roomId = roomId;
      sendJoinResult(connectionId, pending->second.kind, failure);
      _pendingJoins.erase(pending);
    }
    room->setState(RoomState::Waiting);
    _roomManager.deleteRoom(roomId);
  }
  // A persistent room stays listed, its worker is recreated at the next join.
  destroyRoomWorker(roomId);
}

void Game::broadcastRoomState(std::uint32_t roomId) {
  auto *room = _roomManager.getRoom(roomId);
  if (!room)
    return;
  protocol::RoomStateUpdatePayload payload{};
  payload.roomId = roomId;
  for (const auto connectionId : room->getPlayers()) {
    protocol::RoomPlayerState player{};
    player.connectionId = connectionId;
    player.ready = _roomManager.isPlayerReady(connectionId);
    payload.players.push_back(player);
  }
  for (const auto connectionId : room->getPlayers()) {
    _server.sendRoomStateUpdate(connectionId, payload);
  }
}

void Game::handleRoomRequests() {
  Server::RoomRequestEvent event;
  while (_server.pollRoomRequest(event)) {
    const auto connectionId = event.connectionId;
    switch (event.type) {
    case Server::RoomRequestEvent::Type::ListRooms: {
      protocol::RoomListPayload payload{};
      for (const auto roomId : _roomManager.listRooms()) {
        auto *room = _roomManager.getRoom(roomId);
        if (!room)
          continue;
        protocol::RoomInfoPayload info{};
        info.roomId = roomId;
        info.name = room->getName();
        info.status = static_cast<protocol::RoomStatus>(room->getState());
        info.playerCount = static_cast<std::uint8_t>(room->getPlayers().size());
        info.maxPlayers = static_cast<std::uint8_t>(MAX_PLAYER_PER_ROOM);
        payload.rooms.push_back(info);
        if (payload.rooms.size() == protocol::MAX_ROOMS_PER_LIST)
          break;
      }
      _server.sendRoomList(connectionId, payload);
      break;
    }
    case Server::RoomRequestEvent::Type::CreateRoom: {
      protocol::RoomResultPayload res{};
      if (_roomManager.getRoomId(connectionId)) {
        res.result = protocol::RoomResult::AlreadyInRoom;
        _server.sendRoomCreated(connectionId, res);
        break;
      }
      freeRoomListSlot();
      const auto roomId = _roomManager.createRoom(event.roomName);
      if (!_roomManager.joinRoom(connectionId, roomId)) {
        _roomManager.deleteRoom(roomId);
        res.result = protocol::RoomResult::InternalError;
        _server.sendRoomCreated(connectionId, res);
        break;
      }
      auto *worker = createRoomWorker(roomId);
      if (!worker) {
        _roomManager.leaveRoom(connectionId);
        _roomManager.deleteRoom(roomId);
        res.result = protocol::RoomResult::InternalError;
        _server.sendRoomCreated(connectionId, res);
        break;
      }
      beginJoin(connectionId, roomId, *worker, JoinKind::Create);
      break;
    }
    case Server::RoomRequestEvent::Type::JoinRoom: {
      protocol::RoomResultPayload res{};
      res.roomId = event.roomId;
      if (_roomManager.getRoomId(connectionId)) {
        res.result = protocol::RoomResult::AlreadyInRoom;
        _server.sendRoomJoined(connectionId, res);
        break;
      }
      auto *room = _roomManager.getRoom(event.roomId);
      if (!room) {
        res.result = protocol::RoomResult::RoomNotFound;
        _server.sendRoomJoined(connectionId, res);
        break;
      }
      if (room->getState() != RoomState::Waiting) {
        res.result = protocol::RoomResult::RoomAlreadyPlaying;
        _server.sendRoomJoined(connectionId, res);
        break;
      }
      if (room->isFull()) {
        res.result = protocol::RoomResult::RoomFull;
        _server.sendRoomJoined(connectionId, res);
        break;
      }
      if (!_roomManager.joinRoom(connectionId, event.roomId)) {
        res.result = protocol::RoomResult::InternalError;

        _server.sendRoomJoined(connectionId, res);
        break;
      }
      auto *worker = createRoomWorker(event.roomId);
      if (!worker) {
        _roomManager.leaveRoom(connectionId);
        res.result = protocol::RoomResult::InternalError;
        _server.sendRoomJoined(connectionId, res);
        break;
      }
      beginJoin(connectionId, event.roomId, *worker, JoinKind::Join);
      break;
    }
    case Server::RoomRequestEvent::Type::LeaveRoom: {
      protocol::RoomResultPayload res{};
      auto roomId = _roomManager.getRoomId(connectionId);
      if (!roomId) {
        res.result = protocol::RoomResult::NotInRoom;
        _server.sendRoomLeft(connectionId, res);
        break;
      }
      res.roomId = *roomId;
      if (!removePlayerFromRoom(connectionId, true)) {
        res.result = protocol::RoomResult::InternalError;
        _server.sendRoomLeft(connectionId, res);
        break;
      }
      broadcastRoomNotice(*roomId,
                          displayName(connectionId) + " LEFT THE ROOM");
      break;
    }
    case Server::RoomRequestEvent::Type::SetReady: {
      auto roomId = _roomManager.getRoomId(connectionId);
      if (!roomId)
        break;
      if (!_roomManager.setPlayerReady(connectionId, event.ready))
        break;
      std::cout << "[Room " << *roomId << "] Player " << connectionId
                << (event.ready ? " READY" : " NOT READY") << std::endl;
      broadcastRoomState(*roomId);
      startRoomIfReady(*roomId);
      break;
    }
    case Server::RoomRequestEvent::Type::Chat:
      broadcastChatMessage(connectionId, event.chatText);
      break;
    case Server::RoomRequestEvent::Type::SetUsername:
      _usernames.insert_or_assign(connectionId, event.username);
      break;
    case Server::RoomRequestEvent::Type::GetScoreboard:
      _server.sendScoreboard(connectionId, scoreboardText());
      break;
    }
  }
}

void Game::saveScores(
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> &scores) {
  bool changed = false;
  for (const auto &[connectionId, score] : scores) {
    const auto username = _usernames.find(connectionId);
    if (username == _usernames.end())
      continue;
    auto &stats = _stats[username->second];
    if (!stats.is_object())
      stats = nlohmann::json::object();
    stats["best"] = std::max(stats.value("best", 0u), score);
    stats["total"] = stats.value("total", 0u) + score;
    stats["games"] = stats.value("games", 0u) + 1;
    changed = true;
  }
  if (changed)
    std::ofstream("scores.json") << _stats.dump(2) << std::endl;
}

std::string Game::scoreboardText() const {
  std::vector<std::pair<std::uint32_t, std::string>> sorted;
  for (const auto &[name, stats] : _stats.items()) {
    if (stats.is_object())
      sorted.emplace_back(stats.value("best", 0u), name);
  }
  std::sort(sorted.rbegin(), sorted.rend());

  std::string text;
  for (std::size_t i = 0; i < sorted.size() && i < 10; ++i) {
    const auto &stats = _stats.at(sorted[i].second);
    text += std::to_string(i + 1) + ". " + sorted[i].second + "  BEST " +
            std::to_string(stats.value("best", 0u)) + "  TOTAL " +
            std::to_string(stats.value("total", 0u)) + "  GAMES " +
            std::to_string(stats.value("games", 0u)) + "\n";
  }
  return text.empty() ? "NO SCORE YET" : text;
}

void Game::broadcastChatMessage(std::uint32_t connectionId,
                                const std::string &text) {
  constexpr auto minimumInterval = std::chrono::milliseconds(250);
  const auto roomId = _roomManager.getRoomId(connectionId);
  if (!roomId || _pendingJoins.contains(connectionId))
    return;
  auto *room = _roomManager.getRoom(*roomId);
  if (!room)
    return;

  const auto now = std::chrono::steady_clock::now();
  const auto last = _lastChatMessage.find(connectionId);
  if (last != _lastChatMessage.end() && now - last->second < minimumInterval)
    return;
  _lastChatMessage.insert_or_assign(connectionId, now);

  protocol::ChatMessagePayload payload{};
  payload.roomId = *roomId;
  payload.senderId = connectionId;
  payload.text = text;
  for (const auto playerId : room->getPlayers()) {
    if (_pendingJoins.contains(playerId))
      continue;
    _server.sendChatMessage(playerId, payload);
  }
  std::cout << "[Room " << *roomId << "] Chat from " << connectionId << ": "
            << text << std::endl;
}

void Game::broadcastRoomNotice(std::uint32_t roomId, const std::string &text) {
  auto *room = _roomManager.getRoom(roomId);
  if (!room)
    return;

  protocol::ChatMessagePayload payload{};
  payload.roomId = roomId;
  payload.senderId = protocol::SERVER_SENDER_ID;
  payload.text = text;
  for (const auto playerId : room->getPlayers()) {
    if (_pendingJoins.contains(playerId))
      continue;
    _server.sendChatMessage(playerId, payload);
  }
  std::cout << "[Room " << roomId << "] Notice: " << text << std::endl;
}

std::string Game::displayName(std::uint32_t connectionId) const {
  const auto it = _usernames.find(connectionId);
  if (it != _usernames.end())
    return it->second;
  return "P" + std::to_string(connectionId);
}

void Game::updateNetworkMetrics() {
  using Clock = std::chrono::steady_clock;
  const auto now = Clock::now();
  const auto metrics = _server.getMetrics();

  if (!_metricsInitialized) {
    _previousMetrics = metrics;
    _lastMetricsTime = now;
    _metricsInitialized = true;
    return;
  }
  const double elapsed =
      std::chrono::duration<double>(now - _lastMetricsTime).count();

  if (elapsed < 1.0)
    return;

  const auto bytesIn = metrics.bytesReceived - _previousMetrics.bytesReceived;

  const auto bytesOut = metrics.bytesSent - _previousMetrics.bytesSent;

  const auto packetsIn =
      metrics.packetsReceived - _previousMetrics.packetsReceived;

  const auto packetsOut = metrics.packetsSent - _previousMetrics.packetsSent;

  const auto retransmissions =
      metrics.retransmissions - _previousMetrics.retransmissions;

  const auto duplicates =
      metrics.packetsDuplicated - _previousMetrics.packetsDuplicated;

  const auto outOfOrder = metrics.packetsOut - _previousMetrics.packetsOut;

  const double bandwidthIn = static_cast<double>(bytesIn) / elapsed / 1024.0;
  const double bandwidthOut = static_cast<double>(bytesOut) / elapsed / 1024.0;
  const double packetsInPerSecond = static_cast<double>(packetsIn) / elapsed;
  const double packetsOutPerSecond = static_cast<double>(packetsOut) / elapsed;
  std::ostringstream report;
  report << std::fixed << std::setprecision(2);
  report

      << "Bandwidth IN: " << bandwidthIn << " KiB/s\n"
      << "Bandwidth OUT: " << bandwidthOut << " KiB/s\n"
      << "Packets IN: " << packetsInPerSecond << " pkt/s\n"
      << "Packets OUT: " << packetsOutPerSecond << " pkt/s\n"
      << "RTT: " << metrics.rtt << " ms\n"
      << "Smooth RTT: " << metrics.smoothRtt << " ms\n"
      << "Jitter: " << metrics.jitter << " ms\n"
      << "Retransmissions: " << retransmissions << "\n"

      << "Duplicates: " << duplicates << "\n"

      << "Out of order: " << outOfOrder << "\n\n";

  const auto clients = _server.getAllClientMetrics();

  for (const auto &[connectionId, metrics] : clients) {

    const auto previousIt = _previousClientMetrics.find(connectionId);
    if (previousIt == _previousClientMetrics.end()) {
      continue;
    }

    const auto &previous = previousIt->second;

    if (metrics.bytesSent < previous.bytesSent ||
        metrics.bytesReceived < previous.bytesReceived ||
        metrics.packetsSent < previous.packetsSent ||
        metrics.packetsReceived < previous.packetsReceived)
      continue;

    const double bandwidthIn =
        static_cast<double>(metrics.bytesReceived - previous.bytesReceived) /
        elapsed / 1024.0;

    const double bandwidthOut =
        static_cast<double>(metrics.bytesSent - previous.bytesSent) / elapsed /
        1024.0;

    const double packetsIn = static_cast<double>(metrics.packetsReceived -
                                                 previous.packetsReceived) /
                             elapsed;

    const double packetsOut =
        static_cast<double>(metrics.packetsSent - previous.packetsSent) /
        elapsed;

    const auto retransmissions =
        static_cast<std::uint64_t>(metrics.retransmissions) -
        previous.retransmissions;

    report << "\n CLIENT " << connectionId << ":\n"
           << "Bandwidth IN  : " << bandwidthIn << " KiB/s\n"
           << "Bandwidth OUT : " << bandwidthOut << " KiB/s\n"
           << "Packets IN    : " << packetsIn << " pkt/s\n"
           << "Packets OUT   : " << packetsOut << " pkt/s\n"
           << "RTT           : " << metrics.rtt << " ms\n"
           << "Smooth RTT    : " << metrics.smoothRtt << " ms\n"
           << "Jitter        : " << metrics.jitter << " ms\n"
           << "Retransmits   : " << retransmissions << "\n"
           << "Duplicates    : " << metrics.packetsDuplicated << "\n"
           << "Out of order  : " << metrics.packetsOut << "\n\n\n";
  }
  std::cout << report.str();
  _previousClientMetrics = clients;

  _previousMetrics = metrics;
  _lastMetricsTime = now;
}

} // namespace rtype::server
