#include "RoomWorker.hpp"

#include "Protocol/RoomPayload.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <exception>
#include <string>
#include <utility>
#include <variant>

namespace rtype::server {

namespace {
using Clock = std::chrono::steady_clock;

constexpr auto TICK_DURATION = std::chrono::nanoseconds(1000000000 / 60);
constexpr float MAX_DELTA_TIME = 0.25f;
constexpr std::uint32_t TICKS_PER_SNAPSHOT = 3;
} // namespace

RoomWorker::RoomWorker(std::uint32_t roomId, Server &server,
                       RoomEventQueue &events)
    : _roomId(roomId), _server(server), _events(events),
      _instance(roomId, server, events),
      _mailbox(std::make_shared<RoomMailbox>()),
      _startedFuture(_started.get_future()), _thread([this]() { run(); }) {}

RoomWorker::~RoomWorker() { stop(); }

void RoomWorker::waitUntilStarted() { _startedFuture.get(); }

void RoomWorker::requestStop() { _mailbox->requestStop(); }

void RoomWorker::stop() {
  requestStop();
  if (_thread.joinable())
    _thread.join();
}

void RoomWorker::run() {
  const auto reportCrash = [this](const std::string &error) {
    RoomEvent event{.type = RoomEvent::Type::Crashed, .roomId = _roomId};
    event.error = error;
    _events.push(std::move(event));
  };

  try {
    _instance.start(_server);
    _started.set_value();
  } catch (const std::exception &error) {
    _started.set_exception(std::current_exception());
    reportCrash(error.what());
    return;
  } catch (...) {
    _started.set_exception(std::current_exception());
    reportCrash("unknown exception");
    return;
  }

  try {
    auto previousTime = Clock::now();
    auto nextTick = previousTime + TICK_DURATION;
    bool stopRequested = false;

    while (!stopRequested) {
      const auto now = Clock::now();
      const float elapsed =
          std::chrono::duration<float>(now - previousTime).count();
      previousTime = now;

      processMailbox();
      tick(std::min(elapsed, MAX_DELTA_TIME));
      _server.flushAll();

      const auto tickEnd = Clock::now();
      nextTick = std::max(nextTick, tickEnd);
      stopRequested = _mailbox->waitForStop(nextTick);
      nextTick += TICK_DURATION;
    }
    _instance.stop();
  } catch (const std::exception &error) {
    reportCrash(error.what());
  } catch (...) {
    reportCrash("unknown exception");
  }
}

void RoomWorker::tick(float dt) {
  _instance.update(dt);
  if (!_instance.isActive())
    return;
  _instance.incrementServerTick();
  if (_instance.getServerTick() % TICKS_PER_SNAPSHOT == 0)
    _instance.sendSnapshots(_server);
}

void RoomWorker::processMailbox() {
  _mailbox->drain(_batch);

  for (const auto &command : _batch.commands) {
    if (const auto *add = std::get_if<AddPlayerCommand>(&command)) {
      RoomEvent event{.type = RoomEvent::Type::PlayerAdded, .roomId = _roomId};
      event.connectionId = add->connectionId;
      event.requestId = add->requestId;
      event.success = _instance.addConnectedPlayer(add->connectionId, _server);
      _events.push(std::move(event));
    } else if (const auto *remove =
                   std::get_if<RemovePlayerCommand>(&command)) {
      _instance.removeConnectedPlayer(remove->connectionId, _server);
      if (remove->notifyClient) {
        protocol::RoomResultPayload result{};
        result.result = protocol::RoomResult::Success;
        result.roomId = _roomId;
        _server.sendRoomLeft(remove->connectionId, result);
      }
    } else {
      _instance.requestGameStart();
    }
  }

  for (const auto &input : _batch.inputs)
    _instance.processInput(input, _server);
  for (const auto &query : _batch.queries)
    _instance.processEntityQuery(query, _server);
}

} // namespace rtype::server
