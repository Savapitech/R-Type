#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>


namespace rtype::server {

  inline constexpr std::size_t MAX_PLAYER_PER_ROOM = 4;

  enum class RoomState {
    Waiting,
    Playing,
    Finished
  };

  class Room {
    public:
      Room(std::uint32_t id, std::string &name, std::size_t maxPlayers = MAX_PLAYER_PER_ROOM);

      bool addPlayer(std::uint32_t connectionId);
      bool removePlayer(std::uint32_t connectionId);
      bool isInside(std::uint32_t connectionId) const;
      bool isEmpty() const;
      bool isFull() const;
      std::uint32_t getRoomId() const;
      RoomState getState() const;
      const std::string &getName() const;
      const std::vector<std::uint32_t> getPlayers() const;
      void setState(RoomState state);

    private:
      std::uint32_t _roomId;
      std::string _name;
      std::size_t _maxPlayers;
      RoomState _state = RoomState::Waiting;
      std::vector<std::uint32_t> _players;
  };

} //namespace rtype::server
