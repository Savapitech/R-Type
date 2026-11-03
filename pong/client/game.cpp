#include "game.hpp"

#include <cstring>
#include <iostream>

#include "action.hpp"
#include "components/AABBCollider.hpp"
#include "components/PongComponents.hpp"
#include "components/Transform.hpp"
#include "components/Velocity.hpp"
#include "events/ScoreEvent.hpp"
#include "systems/ColliderSystem.hpp"
#include "systems/InputSystem.hpp"
#include "systems/MovementSystem.hpp"
#include "systems/PongBallSystem.hpp"
#include "systems/PongPaddleSystem.hpp"
#include "systems/PongScoreSystem.hpp"
#include "systems/RenderConcept.hpp"
#include "systems/RenderSystem.hpp"

namespace pong::client {

Game::Game(std::string host, std::string port)
    : _host(std::move(host)), _port(std::move(port)),
      _window({1280.0f, 720.0f}, "Pong"), _adapter(_window),
      _sceneManager(_window, std::in_place_type<PongMenuScene>) {}

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
  createBindings();
  addSystems(context);

  if (auto *gameScene = _sceneManager.get<PongGameScene>()) {
    gameScene->setBallSystem(_ballSystem);
  }

  try {
    asio::ip::udp::resolver resolver(_ioContext);
    auto endpoints = resolver.resolve(asio::ip::udp::v4(), _host, _port);
    _serverEndpoint = *endpoints.begin();
    _socket = std::make_unique<asio::ip::udp::socket>(_ioContext);
    _socket->open(asio::ip::udp::v4());
    _socket->non_blocking(true);

    pong::protocol::ClientConnectMsg connectMsg;
    asio::error_code ec;
    _socket->send_to(asio::buffer(&connectMsg, sizeof(connectMsg)),
                     _serverEndpoint, 0, ec);
    std::cout << "[Pong Client] Connecting to server at " << _host << ":"
              << _port << "..." << std::endl;
  } catch (const std::exception &e) {
    std::cout << "[Pong Client] Running in standalone local mode (" << e.what()
              << ")" << std::endl;
  }

  _sceneManager.start(context);
}

void Game::registerComponents(rtk::EngineContext &context) {
  context.registry.register_component<::component::Transform>();
  context.registry.register_component<::component::Velocity>();
  context.registry.register_component<::component::AABBCollider>();
  context.registry.register_component<rtk::ecs::concepts::SpriteData>();
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
  context._entityFactory.registerBuilder("collider",
                                         ::component::aabbColliderFromJson);
  context._entityFactory.registerBuilder("Collider",
                                         ::component::aabbColliderFromJson);
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
      "sprite", [this](rtk::ecs::Registry &reg, std::size_t entity,
                       const nlohmann::json &data) {
        rtk::ecs::concepts::SpriteData sprite{};
        const std::string texPath =
            data.value("texture", "pong/asset/texture/PongAssetSheet.png");

        sprite.textureId = _window.loadTexture(texPath.c_str()).getHandle();
        sprite.size = {data["size"].value("w", 0.0f),
                       data["size"].value("h", 0.0f)};
        sprite.origin = {data["origin"].value("x", 0.0f),
                         data["origin"].value("y", 0.0f)};
        sprite.textureRect = {data["textureRect"].value<uint16_t>("x", 0),
                              data["textureRect"].value<uint16_t>("y", 0),
                              data["textureRect"].value<uint16_t>("w", 0),
                              data["textureRect"].value<uint16_t>("h", 0)};
        sprite.color = {data["color"].value<uint8_t>("r", 255),
                        data["color"].value<uint8_t>("g", 255),
                        data["color"].value<uint8_t>("b", 255),
                        data["color"].value<uint8_t>("a", 255)};
        sprite.layer = data.value("layer", 0);
        sprite.flags = data.value("flags", 0);

        reg.get_components<rtk::ecs::concepts::SpriteData>().insert_at(entity,
                                                                       sprite);
      });
}

void Game::loadBlueprints(rtk::EngineContext &context) {
  context._entityFactory.loadBlueprintFromPath(
      "wall_top", "pong/asset/description/wall_top.json");
  context._entityFactory.loadBlueprintFromPath(
      "wall_bottom", "pong/asset/description/wall_bottom.json");
  context._entityFactory.loadBlueprintFromPath(
      "center_dash", "pong/asset/description/center_dash.json");
  context._entityFactory.loadBlueprintFromPath(
      "paddle_left", "pong/asset/description/paddle_left.json");
  context._entityFactory.loadBlueprintFromPath(
      "paddle_right", "pong/asset/description/paddle_right.json");
  context._entityFactory.loadBlueprintFromPath(
      "ball", "pong/asset/description/ball.json");
  context._entityFactory.loadBlueprintFromPath(
      "score_p1", "pong/asset/description/score_p1.json");
  context._entityFactory.loadBlueprintFromPath(
      "score_p2", "pong/asset/description/score_p2.json");
  context._entityFactory.loadBlueprintFromPath(
      "menu_prompt", "pong/asset/description/menu_prompt.json");
}

void Game::createBindings() {
  _inputMapping.bindKey(pong::Input::actionIndex(pong::Action::P1Up),
                        rtk::ecs::concepts::Key::W);
  _inputMapping.bindKey(pong::Input::actionIndex(pong::Action::P1Up),
                        rtk::ecs::concepts::Key::Z);

  _inputMapping.bindKey(pong::Input::actionIndex(pong::Action::P1Down),
                        rtk::ecs::concepts::Key::S);

  _inputMapping.bindKey(pong::Input::actionIndex(pong::Action::P2Up),
                        rtk::ecs::concepts::Key::Up);

  _inputMapping.bindKey(pong::Input::actionIndex(pong::Action::P2Down),
                        rtk::ecs::concepts::Key::Down);

  _inputMapping.bindKey(pong::Input::actionIndex(pong::Action::Confirm),
                        rtk::ecs::concepts::Key::Space);
  _inputMapping.bindKey(pong::Input::actionIndex(pong::Action::Confirm),
                        rtk::ecs::concepts::Key::Enter);

  _inputMapping.bindKey(pong::Input::actionIndex(pong::Action::Back),
                        rtk::ecs::concepts::Key::Escape);
}

void Game::addSystems(rtk::EngineContext &context) {
  context.scheduler.add<rtk::systems::InputSystem<RtkWindowAdapter>>(
      rtk::ecs::Order::Input, std::ref(_adapter), _inputState, _inputMapping);

  _paddleSystem = &context.scheduler.add<pong::systems::PongPaddleSystem>(
      rtk::ecs::Order::Update, std::ref(_inputState));

  _ballSystem = &context.scheduler.add<pong::systems::PongBallSystem>(
      rtk::ecs::Order::Update, std::ref(context.eventBus));

  context.scheduler.add<pong::systems::PongScoreSystem>(
      rtk::ecs::Order::Update, std::ref(context.registry),
      std::ref(context.eventBus));

  context.scheduler.add<rtk::systems::MovementSystem>(rtk::ecs::Order::Physics);

  context.scheduler.add<rtk::systems::ColliderSystem>(
      rtk::ecs::Order::Physics, _collisionBuffer, 64.0f, context.eventBus);

  context.scheduler.add<rtk::systems::RenderSystem<RtkWindowAdapter>>(
      rtk::ecs::Order::Render, std::ref(_adapter));
}

bool Game::shouldContinue() const {
  return !_inputState.closeRequested && !_shouldQuit;
}

void Game::onUpdate(rtk::EngineContext &context, float dt) {
  pollServer(context);
  sendClientInput();

  if (_matchStarted) {
    if (auto *menuScene = _sceneManager.get<PongMenuScene>()) {
      menuScene->setMatchStarted(true);
    }
  }

  if (auto *gameScene = _sceneManager.get<PongGameScene>()) {
    gameScene->setBallSystem(_ballSystem);
  }
  _sceneManager.update(context, _inputState, dt);

  // Local mode match check: if a score hits 10, close window
  if (!_connectedToServer && _ballSystem &&
      (_ballSystem->getScoreP1() >= 10 || _ballSystem->getScoreP2() >= 10)) {
    std::cout
        << "[Pong] Match ended at 10 points! Closing game window." << std::endl;
    _shouldQuit = true;
  }
}

void Game::pollServer(rtk::EngineContext &context) {
  if (!_socket)
    return;

  _reconnectTimer += 0.016f;
  if (!_connectedToServer && _reconnectTimer >= 0.5f) {
    _reconnectTimer = 0.0f;
    pong::protocol::ClientConnectMsg connectMsg;
    asio::error_code ec;
    _socket->send_to(asio::buffer(&connectMsg, sizeof(connectMsg)),
                     _serverEndpoint, 0, ec);
  }

  asio::ip::udp::endpoint sender;
  std::array<std::uint8_t, 256> buf;
  asio::error_code ec;

  while (true) {
    std::size_t len = _socket->receive_from(asio::buffer(buf), sender, 0, ec);
    if (ec)
      break;
    if (len < sizeof(pong::protocol::Header))
      continue;

    pong::protocol::Header hdr;
    std::memcpy(&hdr, buf.data(), sizeof(hdr));
    if (hdr.magic != pong::protocol::PONG_MAGIC)
      continue;

    if (hdr.type == pong::protocol::PacketType::ServerAssignSlot) {
      if (len >= sizeof(pong::protocol::ServerAssignSlotMsg)) {
        pong::protocol::ServerAssignSlotMsg msg;
        std::memcpy(&msg, buf.data(), sizeof(msg));
        _playerSlot = msg.playerSlot;
        _connectedToServer = true;
        if (_ballSystem) {
          _ballSystem->setEnabled(false);
        }
        if (_paddleSystem) {
          _paddleSystem->setEnabled(false);
        }
        std::cout << "[Pong Client] Connected to server! Assigned Slot: "
                  << static_cast<int>(_playerSlot) << std::endl;
        if (msg.matchState == pong::protocol::MatchState::InGame) {
          _matchStarted = true;
          if (auto *menu = _sceneManager.get<PongMenuScene>()) {
            menu->setMatchStarted(true);
          }
        }
      }
    } else if (hdr.type == pong::protocol::PacketType::ServerSnapshot) {
      if (len >= sizeof(pong::protocol::ServerSnapshotMsg)) {
        pong::protocol::ServerSnapshotMsg snap;
        std::memcpy(&snap, buf.data(), sizeof(snap));
        _connectedToServer = true;
        if (_ballSystem) {
          _ballSystem->setEnabled(false);
        }
        if (_paddleSystem) {
          _paddleSystem->setEnabled(false);
        }

        if (snap.matchState == pong::protocol::MatchState::InGame) {
          if (!_matchStarted) {
            _matchStarted = true;
            std::cout << "[Pong Client] Server match started! Entering game."
                      << std::endl;
          }
          if (auto *menu = _sceneManager.get<PongMenuScene>()) {
            menu->setMatchStarted(true);
          }
        }

        applySnapshot(context, snap);

        // Update score display if scores changed
        if (snap.scoreP1 != _latestScoreP1 || snap.scoreP2 != _latestScoreP2) {
          _latestScoreP1 = snap.scoreP1;
          _latestScoreP2 = snap.scoreP2;
          context.eventBus.publish(pong::events::ScoreChangedEvent{
              .scorer = pong::component::Paddle::Side::Left,
              .scoreP1 = _latestScoreP1,
              .scoreP2 = _latestScoreP2});
        }

        // Window closes automatically when score reaches 10
        if (snap.gameOver != 0 || snap.scoreP1 >= 10 || snap.scoreP2 >= 10) {
          std::cout << "[Pong Client] Match finished! A player reached 10 "
                       "points. Closing window."
                    << std::endl;
          _shouldQuit = true;
        }
      }
    }
  }
}

void Game::sendClientInput() {
  if (!_socket || !_connectedToServer)
    return;

  pong::protocol::ClientInputMsg inputMsg;
  inputMsg.playerSlot = _playerSlot;
  std::uint8_t mask = 0;

  // If slot 0, control Left paddle via W/Z and S or Up/Down
  // If slot 1, control Right paddle via Up/Down or W/S
  if (_playerSlot == 0) {
    if (pong::Input::hasAction(_inputState.actionMask, pong::Action::P1Up) ||
        pong::Input::hasAction(_inputState.actionMask, pong::Action::P2Up))
      mask |= 0x01;
    if (pong::Input::hasAction(_inputState.actionMask, pong::Action::P1Down) ||
        pong::Input::hasAction(_inputState.actionMask, pong::Action::P2Down))
      mask |= 0x02;
  } else {
    if (pong::Input::hasAction(_inputState.actionMask, pong::Action::P2Up) ||
        pong::Input::hasAction(_inputState.actionMask, pong::Action::P1Up))
      mask |= 0x01;
    if (pong::Input::hasAction(_inputState.actionMask, pong::Action::P2Down) ||
        pong::Input::hasAction(_inputState.actionMask, pong::Action::P1Down))
      mask |= 0x02;
  }

  inputMsg.inputMask = mask;
  asio::error_code ec;
  _socket->send_to(asio::buffer(&inputMsg, sizeof(inputMsg)), _serverEndpoint,
                   0, ec);
}

void Game::applySnapshot(rtk::EngineContext &context,
                         const pong::protocol::ServerSnapshotMsg &snap) {
  auto &transforms = context.registry.get_components<::component::Transform>();
  auto &velocities = context.registry.get_components<::component::Velocity>();
  auto &paddles = context.registry.get_components<pong::component::Paddle>();

  auto paddleView =
      context.registry.view<::component::Transform, pong::component::Paddle>();
  for (auto ent : paddleView) {
    auto *trans = transforms.get(ent);
    auto *pad = paddles.get(ent);
    if (!trans || !pad)
      continue;
    if (pad->side == pong::component::Paddle::Side::Left) {
      trans->position.y = snap.paddleLeftY;
    } else {
      trans->position.y = snap.paddleRightY;
    }
    if (auto *vel = velocities.get(ent)) {
      vel->x = 0.0f;
      vel->y = 0.0f;
    }
  }

  auto ballView =
      context.registry.view<::component::Transform, pong::component::Ball>();
  for (auto ent : ballView) {
    auto *trans = transforms.get(ent);
    if (!trans)
      continue;
    trans->position.x = snap.ballX;
    trans->position.y = snap.ballY;
    if (auto *vel = velocities.get(ent)) {
      vel->x = 0.0f;
      vel->y = 0.0f;
    }
  }

  // Authoritative server score sync directly to sprite digit rendering
  auto scoreView =
      context.registry.view<pong::component::ScoreDisplay,
                            rtk::ecs::concepts::SpriteData>();
  auto &scores =
      context.registry.get_components<pong::component::ScoreDisplay>();
  auto &sprites =
      context.registry.get_components<rtk::ecs::concepts::SpriteData>();

  for (auto ent : scoreView) {
    auto *sc = scores.get(ent);
    auto *sp = sprites.get(ent);
    if (!sc || !sp)
      continue;
    const std::uint8_t val = (sc->side == pong::component::Paddle::Side::Left)
                                 ? snap.scoreP1
                                 : snap.scoreP2;
    sc->score = val;
    sp->textureRect = pong::systems::PongScoreSystem::DIGIT_RECTS
        [std::min<std::size_t>(val, 9)];
  }
}

void Game::onUnload(rtk::EngineContext &context) {
  _sceneManager.stop(context);
  if (_socket && _socket->is_open()) {
    asio::error_code ec;
    _socket->close(ec);
  }
}

} // namespace pong::client
