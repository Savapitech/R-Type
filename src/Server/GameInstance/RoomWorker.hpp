#pragma once

#include "GameInstance.hpp"
#include "RoomChannels.hpp"

#include <cstdint>
#include <future>
#include <memory>
#include <thread>

namespace rtype::server {

/**
 * @brief Runs the simulation of one room on its own thread.
 */
class RoomWorker {
public:
  /**
   * @brief Creates the instance and starts the room thread.
   * @param roomId Identifier of the room.
   * @param server Shared network server, must outlive the worker.
   * @param events Queue read by the main thread, must outlive the worker.
   * @throws std::system_error If the thread cannot be started.
   */
  RoomWorker(std::uint32_t roomId, Server &server, RoomEventQueue &events);

  /** @brief Stops and joins the room thread. */
  ~RoomWorker();

  RoomWorker(const RoomWorker &) = delete;
  RoomWorker &operator=(const RoomWorker &) = delete;
  RoomWorker(RoomWorker &&) = delete;
  RoomWorker &operator=(RoomWorker &&) = delete;

  /**
   * @brief Returns the mailbox of the room.
   * @return Shared mailbox, it can outlive the worker.
   */
  const std::shared_ptr<RoomMailbox> &mailbox() const { return _mailbox; }

  /**
   * @brief Blocks until the instance is loaded, can only be called once.
   * @throws Whatever the instance threw while loading.
   */
  void waitUntilStarted();

  /**
   * @brief Asks the room thread to exit without waiting for it.
   */
  void requestStop();

  /**
   * @brief Stops the room thread and waits for it to exit.
   */
  void stop();

private:
  void run();
  void tick(float dt);
  void processMailbox();

  std::uint32_t _roomId;
  Server &_server;
  RoomEventQueue &_events;
  GameInstance _instance;
  std::shared_ptr<RoomMailbox> _mailbox;
  RoomMailbox::Batch _batch;
  std::promise<void> _started;
  std::future<void> _startedFuture;
  std::thread _thread;
};

} // namespace rtype::server
