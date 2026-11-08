#pragma once

#include "Server.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace rtype::server {

/** @brief Asks a room to create the entity of a connection. */
struct AddPlayerCommand {
  std::uint32_t connectionId; ///< Connection joining the room.
  std::uint64_t requestId;    ///< Echoed in the PlayerAdded event.
};

/** @brief Asks a room to destroy the entity of a connection. */
struct RemovePlayerCommand {
  std::uint32_t connectionId; ///< Connection leaving the room.
  bool notifyClient;          ///< Whether to notify the client about removal.
};

/** @brief Asks a room to leave its lobby and start a round. */
struct StartGameCommand {};

/** @brief Order sent by the main thread to a room thread, never dropped. */
using RoomCommand =
    std::variant<AddPlayerCommand, RemovePlayerCommand, StartGameCommand>;

/** @brief Notification sent by a room thread to the main thread. */
struct RoomEvent {
  /** @brief Kind of notification. */
  enum class Type {
    PlayerAdded, ///< An AddPlayerCommand was processed.
    GameEnded,   ///< The round ended, the room is back in its lobby.
    Crashed      ///< An exception stopped the room thread.
  };

  Type type;                     ///< Kind of notification.
  std::uint32_t roomId;          ///< Room sending the notification.
  std::uint32_t connectionId{0}; ///< Player concerned, for PlayerAdded.
  std::uint64_t requestId{0};    ///< Request answered, for PlayerAdded.
  bool success{false};           ///< Outcome, for PlayerAdded.
  std::string error{};           ///< Exception message, for Crashed.
  /// Connection ID and score of each player, for GameEnded.
  std::vector<std::pair<std::uint32_t, std::uint32_t>> scores{};
};

/** @brief Thread safe queue of the notifications sent to the main thread. */
class RoomEventQueue {
public:
  /**
   * @brief Adds a notification.
   * @param event Notification to add.
   */
  void push(RoomEvent event);
  /**
   * @brief Pops the oldest notification.
   * @param event Receives the notification.
   * @return True if a notification was popped.
   */
  bool poll(RoomEvent &event);

private:
  std::mutex _mutex;
  std::queue<RoomEvent> _events;
};

/**
 * @brief Everything a room receives from the other threads.
 *
 * Commands are never dropped. Inputs and queries come straight from the
 * network thread and are bounded per room, so a flooding client can only
 * delay its own room.
 */
class RoomMailbox {
public:
  /** @brief Messages taken out of the mailbox at once. */
  struct Batch {
    std::vector<RoomCommand> commands;             ///< Pending orders.
    std::vector<Server::InputEvent> inputs;        ///< Pending inputs.
    std::vector<Server::QueryEntityEvent> queries; ///< Pending queries.
  };

  /**
   * @brief Adds an order.
   * @param command Order to add.
   */
  void post(RoomCommand command);
  /**
   * @brief Adds an input, dropped if the room has too many pending inputs.
   * @param event Input to add.
   */
  void postInput(const Server::InputEvent &event);
  /**
   * @brief Adds a query, dropped if the room has too many pending queries.
   * @param event Query to add.
   */
  void postQuery(const Server::QueryEntityEvent &event);
  /**
   * @brief Moves every pending message to a batch.
   * @param batch Emptied, then receives the messages.
   */
  void drain(Batch &batch);
  /**
   * @brief Asks the room thread to exit and wakes it up.
   */
  void requestStop();
  /**
   * @brief Sleeps until a deadline or until a stop is requested.
   * @param deadline End of the sleep.
   * @return True if a stop was requested.
   */
  bool waitForStop(std::chrono::steady_clock::time_point deadline);

private:
  std::mutex _mutex;
  std::condition_variable _stopSignal;
  bool _stopRequested = false;
  Batch _pending;
};

/**
 * @brief Tells the network thread which room receives the messages of a
 * connection. Written by the main thread when players join and leave.
 */
class RoomRouter {
public:
  /**
   * @brief Routes the messages of a connection to a room.
   * @param connectionId Connection to route.
   * @param mailbox Mailbox of the room.
   */
  void bind(std::uint32_t connectionId, std::shared_ptr<RoomMailbox> mailbox);
  /**
   * @brief Stops routing the messages of a connection.
   * @param connectionId Connection to forget.
   */
  void unbind(std::uint32_t connectionId);
  /**
   * @brief Looks up the room of a connection.
   * @param connectionId Connection to look up.
   * @return Mailbox of the room, or nullptr if the connection has no room.
   */
  std::shared_ptr<RoomMailbox> find(std::uint32_t connectionId) const;

private:
  mutable std::shared_mutex _mutex;
  std::unordered_map<std::uint32_t, std::shared_ptr<RoomMailbox>> _routes;
};

} // namespace rtype::server
