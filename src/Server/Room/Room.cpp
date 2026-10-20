#include "Room.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rtype::server {

  Room::Room(std::uint32_t id, const std::string &name, std::size_t maxPlayers) :
    _roomId(id), _name(name), _maxPlayers(maxPlayers)
  {}

  bool Room::addPlayer(std::uint32_t connectionId) {
    if (_state != RoomState::Waiting || isFull() || isInside(connectionId))
      return false;
    _players.push_back(connectionId);
    return true;
  }

  bool Room::removePlayer(std::uint32_t connectionId) {
    auto it = std::find(_players.begin(), _players.end(), connectionId);

    if (it == _players.end())
      return false;

    _players.erase(it);
    return true;
  }
  
  bool Room::isInside(std::uint32_t connectionId) const {
    return std::find(_players.begin(), _players.end(), connectionId) != _players.end();
  }

  bool Room::isFull() const {
    return _players.size() >= _maxPlayers;
  }

  bool Room::isEmpty() const {
    return _players.empty();
  }

  std::uint32_t Room::getRoomId() const {
    return _roomId;
  }

  RoomState Room::getState() const {
    return _state;
  }

  const std::string &Room::getName() const {
    return _name;
  }

 const std::vector<std::uint32_t> &Room::getPlayers() const {
   return _players;
 }

 void Room::setState(RoomState state) {
   _state = state;
 }

} //namespace rtype::server
