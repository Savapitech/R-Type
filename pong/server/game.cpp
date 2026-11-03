#include "game.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>

#include "components/PongComponents.hpp"
#include "components/Transform.hpp"
#include "components/Velocity.hpp"
#include "systems/MovementSystem.hpp"

namespace pong::server {

Game::Game(std::uint16_t port) : _port(port) {}

Game::~Game() {
  if (_socket && _socket->is_open()) {
    asio::error_code ec;
    _socket->close(ec);
  }
}

void Game::onLoad(rtk::EngineContext &context) {
  registerComponents(context);
  registerBuilders(context);
  loadBlueprints(context);
  addSystems(context);

  _paddleLeft = context._entityFactory.createFromBlueprint(context.registry,
                                                           "paddle_left");
  _paddleRight = context._entityFactory.createFromBlueprint(context.registry,
                                                            "paddle_right");
  _ball = context._entityFactory.createFromBlueprint(context.registry, "ball");

  if (_ballSystem) {
    _ballSystem->setEnabled(false);
  }

  try {
    _socket = std::make_unique<asio::ip::udp::socket>(_ioContext);
    _socket->open(asio::ip::udp::v4());
    _socket->set_option(asio::ip::udp::socket::reuse_address(true));
    _socket->bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), _port));
    _socket->non_blocking(true);
    std::cout << "[Pong Server] Listening on UDP port " << _port << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "[Pong Server] Error opening socket: " << e.what()
              << std::endl;
    throw;
  }
}

void Game::registerComponents(rtk::EngineContext &context) {
  context.registry.register_component<::component::Transform>();
  context.registry.register_component<::component::Velocity>();
  context.registry.register_component<pong::component::Paddle>();
  context.registry.register_component<pong::component::Ball>();
  context.registry.register_component<pong::component::ScoreDisplay>();
}

void Game::registerBuilders(rtk::EngineContext &context) {
  context._entityFactory.registerBuilder("transform",
                                         ::component::transformFromJson);
  context._entityFactory.registerBuilder("Transform",
                                         ::component::transformFromJson);
  context._entityFactory.registerBuilder("velocity",
                                         ::component::velocityFromJson);
  context._entityFactory.registerBuilder("Velocity",
                                         ::component::velocityFromJson);
  context._entityFactory.registerBuilder("paddle",
                                         pong::component::paddleFromJson);
  context._entityFactory.registerBuilder("Paddle",
                                         pong::component::paddleFromJson);
  context._entityFactory.registerBuilder("ball",
                                         pong::component::ballFromJson);
  context._entityFactory.registerBuilder("Ball",
                                         pong::component::ballFromJson);
  context._entityFactory.registerBuilder(
      "scoreDisplay", pong::component::scoreDisplayFromJson);
  context._entityFactory.registerBuilder(
      "ScoreDisplay", pong::component::scoreDisplayFromJson);

  context._entityFactory.registerBuilder(
      "sprite",
      [](rtk::ecs::Registry &, std::size_t, const nlohmann::json &) {});
}

void Game::loadBlueprints(rtk::EngineContext &context) {
  context._entityFactory.loadBlueprintFromPath(
      "paddle_left", "pong/asset/description/paddle_left.json");
  context._entityFactory.loadBlueprintFromPath(
      "paddle_right", "pong/asset/description/paddle_right.json");
  context._entityFactory.loadBlueprintFromPath(
      "ball", "pong/asset/description/ball.json");
}

void Game::addSystems(rtk::EngineContext &context) {
  _ballSystem = &context.scheduler.add<pong::systems::PongBallSystem>(
      rtk::ecs::Order::Update, std::ref(context.eventBus));

  context.scheduler.add<pong::systems::PongPaddleSystem>(
      rtk::ecs::Order::Update);

  context.scheduler.add<rtk::systems::MovementSystem>(rtk::ecs::Order::Physics);
}

bool Game::shouldContinue() const { return !_shouldQuit; }

void Game::onUpdate(rtk::EngineContext &context, float dt) {
  pollNetwork(context);

  if (_matchState == pong::protocol::MatchState::InGame) {
    auto &velocities = context.registry.get_components<::component::Velocity>();
    auto &transforms = context.registry.get_components<::component::Transform>();

    if (auto *v1 = velocities.get(_paddleLeft)) {
      const bool up = (_p1InputMask & 0x01) != 0;
      const bool down = (_p1InputMask & 0x02) != 0;
      v1->y = (up && !down) ? -520.0f : ((down && !up) ? 520.0f : 0.0f);
    }
    if (auto *t1 = transforms.get(_paddleLeft)) {
      t1->position.y = std::clamp(t1->position.y, 8.0f, 712.0f - 64.0f);
    }

    if (auto *v2 = velocities.get(_paddleRight)) {
      const bool up = (_p2InputMask & 0x01) != 0;
      const bool down = (_p2InputMask & 0x02) != 0;
      v2->y = (up && !down) ? -520.0f : ((down && !up) ? 520.0f : 0.0f);
    }
    if (auto *t2 = transforms.get(_paddleRight)) {
      t2->position.y = std::clamp(t2->position.y, 8.0f, 712.0f - 64.0f);
    }
  }

  _snapshotTimer += dt;
  if (_snapshotTimer >= 0.016f) {
    _snapshotTimer = 0.0f;
    broadcastSnapshot(context);
  }

  if (_ballSystem && _ballSystem->isGameOver()) {
    _matchState = pong::protocol::MatchState::GameOver;
    _gameOverTimer += dt;
    broadcastSnapshot(context);
    if (_gameOverTimer >= 0.2f) {
      std::cout << "[Pong Server] Match ended !" << std::endl;
      _shouldQuit = true;
    }
  }
}

void Game::pollNetwork(rtk::EngineContext &context) {
  if (!_socket)
    return;

  asio::ip::udp::endpoint sender;
  std::array<std::uint8_t, 256> buffer;
  asio::error_code ec;

  while (true) {
    std::size_t bytes =
        _socket->receive_from(asio::buffer(buffer), sender, 0, ec);
    if (ec)
      break;
    handlePacket(context, sender, buffer.data(), bytes);
  }
}

void Game::handlePacket(rtk::EngineContext &context,
                        const asio::ip::udp::endpoint &sender,
                        const std::uint8_t *data, std::size_t size) {
  if (size < sizeof(pong::protocol::Header))
    return;

  pong::protocol::Header header;
  std::memcpy(&header, data, sizeof(header));
  if (header.magic != pong::protocol::PONG_MAGIC)
    return;

  auto now = std::chrono::steady_clock::now();

  if (header.type == pong::protocol::PacketType::ClientConnect) {
    auto it = std::find_if(
        _clients.begin(), _clients.end(),
        [&](const ClientSession &s) { return s.endpoint == sender; });
    std::uint8_t assignedSlot = 0;
    if (it == _clients.end()) {
      if (_clients.empty()) {
        assignedSlot = 0;
      } else if (_clients.size() == 1) {
        assignedSlot = 1;
      } else {
        assignedSlot = 2;
      }
      _clients.push_back({sender, assignedSlot, now});
      std::cout << "[Pong Server] Client connected from "
                << sender.address().to_string() << ":" << sender.port()
                << " assigned Slot " << static_cast<int>(assignedSlot)
                << std::endl;

      if (_clients.size() >= 2 &&
          _matchState == pong::protocol::MatchState::WaitingForPlayers) {
        _matchState = pong::protocol::MatchState::InGame;
        if (_ballSystem) {
          _ballSystem->setEnabled(true);
          _ballSystem->resetMatch(context.registry);
        }
        std::cout << "[Pong Server] 2 players connected! Starting match!"
                  << std::endl;
      }
    } else {
      assignedSlot = it->playerSlot;
      it->lastSeen = now;
    }

    pong::protocol::ServerAssignSlotMsg reply;
    reply.playerSlot = assignedSlot;
    reply.matchState = _matchState;
    asio::error_code ec;
    _socket->send_to(asio::buffer(&reply, sizeof(reply)), sender, 0, ec);
  } else if (header.type == pong::protocol::PacketType::ClientInput) {
    if (size < sizeof(pong::protocol::ClientInputMsg))
      return;
    pong::protocol::ClientInputMsg msg;
    std::memcpy(&msg, data, sizeof(msg));

    auto it = std::find_if(
        _clients.begin(), _clients.end(),
        [&](const ClientSession &s) { return s.endpoint == sender; });
    if (it != _clients.end()) {
      it->lastSeen = now;
      if (it->playerSlot == 0) {
        _p1InputMask = msg.inputMask;
      } else if (it->playerSlot == 1) {
        _p2InputMask = msg.inputMask;
      }
    }
  }
}

void Game::broadcastSnapshot(rtk::EngineContext &context) {
  if (!_socket || _clients.empty())
    return;

  auto &transforms = context.registry.get_components<::component::Transform>();

  pong::protocol::ServerSnapshotMsg snap;
  snap.matchState = _matchState;
  if (auto *tLeft = transforms.get(_paddleLeft)) {
    snap.paddleLeftY = tLeft->position.y;
  }
  if (auto *tRight = transforms.get(_paddleRight)) {
    snap.paddleRightY = tRight->position.y;
  }
  if (auto *tBall = transforms.get(_ball)) {
    snap.ballX = tBall->position.x;
    snap.ballY = tBall->position.y;
  }
  if (_ballSystem) {
    snap.scoreP1 = _ballSystem->getScoreP1();
    snap.scoreP2 = _ballSystem->getScoreP2();
    snap.gameOver = _ballSystem->isGameOver() ? 1 : 0;
  }

  asio::error_code ec;
  for (const auto &client : _clients) {
    _socket->send_to(asio::buffer(&snap, sizeof(snap)), client.endpoint, 0, ec);
  }
}

void Game::onUnload(rtk::EngineContext &) {
  if (_socket && _socket->is_open()) {
    asio::error_code ec;
    _socket->close(ec);
  }
}

} // namespace pong::server
