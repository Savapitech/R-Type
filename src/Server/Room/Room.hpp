#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>


namespace rtype::server {

  /** @brief Default maximum number of players in a room. */
  inline constexpr std::size_t MAX_PLAYER_PER_ROOM = 4;

  /** @brief Lifecycle state of a room. */
  enum class RoomState {
    Waiting,
    Playing,
    Finished
  };

  /** @brief Represents a group of players sharing one game session. */
  class Room {
    public:
      /**
       * @brief Creates a room with a player capacity.
       * @param id Unique room identifier.
       * @param name Display name of the room.
       * @param maxPlayers Maximum number of players.
       */
      Room(std::uint32_t id, const std::string &name, std::size_t maxPlayers = MAX_PLAYER_PER_ROOM);

      /**
       * @brief Adds a connection while the room is waiting and has space.
       * @param connectionId Connection to add.
       * @return True if the player was added.
       */
      bool addPlayer(std::uint32_t connectionId);
      /**
       * @brief Removes a connection from the room.
       * @param connectionId Connection to remove.
       * @return True if the player was removed.
       */
      bool removePlayer(std::uint32_t connectionId);
      /**
       * @brief Checks whether a connection belongs to this room.
       * @param connectionId Connection to look up.
       * @return True if the connection is present.
       */
      bool isInside(std::uint32_t connectionId) const;
      /**
       * @brief Checks whether the room has no players.
       * @return True if the room is empty.
       */
      bool isEmpty() const;
      /**
       * @brief Checks whether the room has reached its capacity.
       * @return True if the room is full.
       */
      bool isFull() const;
      /**
       * @brief Returns the unique room identifier.
       * @return Room identifier.
       */
      std::uint32_t getRoomId() const;
      /**
       * @brief Returns the current lifecycle state.
       * @return Current room state.
       */
      RoomState getState() const;
      /**
       * @brief Returns the room display name.
       * @return Reference to the room name.
       */
      const std::string &getName() const;
      /**
       * @brief Returns the connection IDs currently in the room.
       * @return Read-only list of connection IDs.
       */
      const std::vector<std::uint32_t> &getPlayers() const;
      /**
       * @brief Changes the room lifecycle state.
       * @param state New room state.
       */
      void setState(RoomState state);

    private:
      std::uint32_t _roomId;
      std::string _name;
      std::size_t _maxPlayers;
      RoomState _state = RoomState::Waiting;
      std::vector<std::uint32_t> _players;
  };

} //namespace rtype::server

