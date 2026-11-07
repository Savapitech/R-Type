#include "RoomChannels.hpp"

#include "config/NetworkConfig.hpp"

#include <utility>

namespace rtype::server {

void RoomEventQueue::push(RoomEvent event) {
  std::lock_guard<std::mutex> lock(_mutex);
  _events.push(std::move(event));
}

bool RoomEventQueue::poll(RoomEvent &event) {
  std::lock_guard<std::mutex> lock(_mutex);
  if (_events.empty())
    return false;
  event = std::move(_events.front());
  _events.pop();
  return true;
}

void RoomMailbox::post(RoomCommand command) {
  std::lock_guard<std::mutex> lock(_mutex);
  _pending.commands.push_back(std::move(command));
}

void RoomMailbox::postInput(const Server::InputEvent &event) {
  std::lock_guard<std::mutex> lock(_mutex);
  if (_pending.inputs.size() < MAX_QUEUED_EVENTS)
    _pending.inputs.push_back(event);
}

void RoomMailbox::postQuery(const Server::QueryEntityEvent &event) {
  std::lock_guard<std::mutex> lock(_mutex);
  if (_pending.queries.size() < MAX_QUEUED_EVENTS)
    _pending.queries.push_back(event);
}

void RoomMailbox::drain(Batch &batch) {
  batch.commands.clear();
  batch.inputs.clear();
  batch.queries.clear();

  std::lock_guard<std::mutex> lock(_mutex);
  batch.commands.swap(_pending.commands);
  batch.inputs.swap(_pending.inputs);
  batch.queries.swap(_pending.queries);
}

void RoomMailbox::requestStop() {
  {
    std::lock_guard<std::mutex> lock(_mutex);
    _stopRequested = true;
  }
  _stopSignal.notify_all();
}

bool RoomMailbox::waitForStop(std::chrono::steady_clock::time_point deadline) {
  std::unique_lock<std::mutex> lock(_mutex);
  return _stopSignal.wait_until(lock, deadline,
                                [this]() { return _stopRequested; });
}

void RoomRouter::bind(std::uint32_t connectionId,
                      std::shared_ptr<RoomMailbox> mailbox) {
  std::unique_lock lock(_mutex);
  _routes.insert_or_assign(connectionId, std::move(mailbox));
}

void RoomRouter::unbind(std::uint32_t connectionId) {
  std::unique_lock lock(_mutex);
  _routes.erase(connectionId);
}

std::shared_ptr<RoomMailbox>
RoomRouter::find(std::uint32_t connectionId) const {
  std::shared_lock lock(_mutex);
  const auto it = _routes.find(connectionId);
  return it == _routes.end() ? nullptr : it->second;
}

} // namespace rtype::server
