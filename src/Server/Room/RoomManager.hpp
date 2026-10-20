#pragma once

#include "Room.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace rtype::server {
  
  class RoomManager {
    public:
      std::uint32_t createRoom(const std::string &name);
      bool joinRoom(std::uint32_t connectionId, std::uint32_t roomId);
      bool leaveRoom(std::uint32_t connectionId);
      Room *getRoom(std::uint32_t roomId);
      std::optional<std::uint32_t> getRoomId(std::uint32_t connectionId) const;
      std::vector<std::uint32_t> listRooms() const;
      bool deleteRoom(std::uint32_t roomId);

    private:
      std::uint32_t _nextRoomId = 1;
      std::unordered_map<std::uint32_t, std::unique_ptr<Room>> _rooms;
      std::unordered_map<std::uint32_t, std::uint32_t> _playersRooms;
  };
}
