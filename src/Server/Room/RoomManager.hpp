#pragma once

#include "Room.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace rtype::server {
  
  /** @brief Owns rooms and tracks the room assigned to each connection. */
  class RoomManager {
    public:
      /**
       * @brief Creates and registers a new room.
       * @param name Display name for the room.
       * @return Identifier of the created room.
       */
      std::uint32_t createRoom(const std::string &name);
      /**
       * @brief Assigns a connection to an existing room.
       * @param connectionId Connection to join.
       * @param roomId Destination room identifier.
       * @return True if the join succeeded.
       */
      bool joinRoom(std::uint32_t connectionId, std::uint32_t roomId);
      /**
       * @brief Removes a connection and deletes its room if empty.
       * @param connectionId Connection to remove.
       * @return True if a membership was removed.
       */
      bool leaveRoom(std::uint32_t connectionId);
      /**
       * @brief Looks up a room by identifier.
       * @param roomId Room identifier.
       * @return Pointer to the room, or nullptr if not found.
       */
      Room *getRoom(std::uint32_t roomId);
      /**
       * @brief Looks up the room containing a connection.
       * @param connectionId Connection to look up.
       * @return Room identifier, or std::nullopt if not assigned.
       */
      std::optional<std::uint32_t> getRoomId(std::uint32_t connectionId) const;
      /**
       * @brief Lists currently registered room identifiers.
       * @return Vector of room identifiers.
       */
      std::vector<std::uint32_t> listRooms() const;
      /**
       * @brief Deletes a room if deletion is allowed.
       * @param roomId Room identifier.
       * @return True if the room was deleted.
       */
      bool deleteRoom(std::uint32_t roomId);

    private:
      std::uint32_t _nextRoomId = 1;
      std::unordered_map<std::uint32_t, std::unique_ptr<Room>> _rooms;
      std::unordered_map<std::uint32_t, std::uint32_t> _playersRooms;
  };
}

