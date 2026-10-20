#include "RoomManager.hpp"
#include "Room.hpp"
#include <algorithm>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>


namespace rtype::server {

  std::uint32_t RoomManager::createRoom(const std::string &name) {
    if (_nextRoomId == 0)
      throw std::overflow_error("No Id for the Room available.");

    const std::uint32_t id = _nextRoomId++;
    _rooms.emplace(id, std::make_unique<Room>(id, name));
    return id;
  }

  bool RoomManager::joinRoom(std::uint32_t connectionId, std::uint32_t roomId) {
    auto exist = _playersRooms.find(connectionId);
    if (exist != _playersRooms.end())
      return false;

    auto it = _rooms.find(roomId);
    if (it == _rooms.end())
      return false;

    Room& room = *(it->second);

    if (!room.addPlayer(connectionId))
      return false;

    try {
      _playersRooms.emplace(connectionId, room.getRoomId());
    } catch (...) {
      room.removePlayer(connectionId);
      throw;
    }
    return true;
  }

  bool RoomManager::leaveRoom(std::uint32_t connectionId) {
    auto itP = _playersRooms.find(connectionId);
    if (itP == _playersRooms.end())
      return false;

    const std::uint32_t roomId = itP->second;

    auto itR = _rooms.find(roomId);
    if (itR == _rooms.end())
      return false;

    Room &room = *(itR->second);

    if (!room.removePlayer(connectionId))
      return false;

    _playersRooms.erase(itP);

    if (room.isEmpty())
      _rooms.erase(itR);

    return true;

  }

  Room *RoomManager::getRoom(std::uint32_t roomId) {
    auto it = _rooms.find(roomId);
    if (it == _rooms.end())
      return nullptr;
    return it->second.get();
  }

  std::optional<std::uint32_t> RoomManager::getRoomId(std::uint32_t connectionId) const {
    auto it = _playersRooms.find(connectionId);
    if (it == _playersRooms.end())
      return std::nullopt;
    return it->second;
  }

  std::vector<std::uint32_t> RoomManager::listRooms() const {
    std::vector<std::uint32_t> roomList;
    roomList.reserve(_rooms.size());

    for (const auto &[id, room] : _rooms) {
      roomList.push_back(id);
    }
    std::sort(roomList.begin(), roomList.end());
    return roomList;
  }

  bool RoomManager::deleteRoom(std::uint32_t roomId) {
    auto it = _rooms.find(roomId);
    if (it == _rooms.end() || !it->second->isEmpty())
      return false;
    _rooms.erase(it);
    return true;
  }

} //namespace rtype::server
