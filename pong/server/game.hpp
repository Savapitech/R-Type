#pragma once

#include <asio.hpp>
#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "EngineContext.hpp"
#include "Game.hpp"
#include "protocol/PongProtocol.hpp"
#include "systems/PongBallSystem.hpp"
#include "systems/PongPaddleSystem.hpp"

namespace pong::server {

/**
 * @brief Authoritative headless server running the Pong match simulation.
 */
class Game {
public:
  /**
   * @brief Creates the server bound to a UDP port.
   * @param port Port to listen on (default: 4242).
   */
  explicit Game(std::uint16_t port = 4242);
  ~Game();

  /**
   * @brief Initializes ECS components, blueprints, systems and opens socket.
   * @param context Engine services.
   */
  void onLoad(rtk::EngineContext &context);

  /**
   * @brief Checks if the server loop should continue running.
   * @return True if running.
   */
  [[nodiscard]] bool shouldContinue() const;

  /**
   * @brief Ticks network polling, client inputs, simulation, and broadcasts snapshots.
   * @param context Engine services.
   * @param dt Elapsed frame time in seconds.
   */
  void onUpdate(rtk::EngineContext &context, float dt);

  /**
   * @brief Shuts down server socket and resources.
   * @param context Engine services.
   */
  void onUnload(rtk::EngineContext &context);

private:
  struct ClientSession {
    asio::ip::udp::endpoint endpoint;
    std::uint8_t playerSlot = 0; // 0: P1, 1: P2, 2+: spectator
    std::chrono::steady_clock::time_point lastSeen{};
  };

  void registerComponents(rtk::EngineContext &context);
  void registerBuilders(rtk::EngineContext &context);
  void loadBlueprints(rtk::EngineContext &context);
  void addSystems(rtk::EngineContext &context);

  void pollNetwork(rtk::EngineContext &context);
  void handlePacket(rtk::EngineContext &context,
                    const asio::ip::udp::endpoint &sender,
                    const std::uint8_t *data, std::size_t size);
  void broadcastSnapshot(rtk::EngineContext &context);

  std::uint16_t _port;
  asio::io_context _ioContext;
  std::unique_ptr<asio::ip::udp::socket> _socket;

  std::vector<ClientSession> _clients;

  std::size_t _paddleLeft = 0;
  std::size_t _paddleRight = 0;
  std::size_t _ball = 0;
  pong::systems::PongBallSystem *_ballSystem = nullptr;

  std::uint8_t _p1InputMask = 0;
  std::uint8_t _p2InputMask = 0;

  pong::protocol::MatchState _matchState =
      pong::protocol::MatchState::WaitingForPlayers;
  bool _shouldQuit = false;
  float _snapshotTimer = 0.0f;
  float _gameOverTimer = 0.0f;
};

} // namespace pong::server
