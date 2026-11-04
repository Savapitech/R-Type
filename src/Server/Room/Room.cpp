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
    _readyStates[connectionId] = false;
    return true;
  }

  bool Room::removePlayer(std::uint32_t connectionId) {
    auto it = std::find(_players.begin(), _players.end(), connectionId);

    if (it == _players.end())
      return false;

    _players.erase(it);
    _readyStates.erase(connectionId);
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

 bool Room::setPlayerReady(std::uint32_t connectionId, bool ready) {
   if (_state != RoomState::Waiting)
      return false;
   auto it = _readyStates.find(connectionId);
   if (it == _readyStates.end())
     return false;
   it->second = ready;
   return true;
 }

 bool Room::isPlayerReady(std::uint32_t connectionId) const {
   auto it = _readyStates.find(connectionId);
   if (it == _readyStates.end())
     return false;
   return it->second;
 }

 bool Room::areAllPlayersReady() const {
   if (_players.empty())
     return false;

   for (const auto connectionId : _players) {
     if (!isPlayerReady(connectionId))
       return false;
   }
   return true;
 }

 void Room::resetReadyStates() {
   for (auto &[_, ready] : _readyStates)
     ready = false;
 }



} //namespace rtype::server
