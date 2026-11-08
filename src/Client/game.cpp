#include "game.hpp"
#include "ClientMetrics.hpp"
#include "EngineContext.hpp"
#include "Logger/Logger.hpp"
#include "NetworkClient.hpp"
#include "Protocol/ChatPayload.hpp"
#include "Protocol/MessageType.hpp"
#include "Protocol/PayloadCodec.hpp"
#include "Protocol/RoomPayload.hpp"
#include "Scheduler.hpp"
#include "components/AABBCollider.hpp"
#include "components/Animation.hpp"
#include "components/Animator.hpp"
#include "components/AttachedTo.hpp"
#include "components/Bullet.hpp"
#include "components/Controllable.hpp"
#include "components/Dead.hpp"
#include "components/DeathEffect.hpp"
#include "components/DestroyAnimEnd.hpp"
#include "components/DestroyableTile.hpp"
#include "components/Enemy.hpp"
#include "components/EnemyEvent.hpp"
#include "components/Health.hpp"
#include "components/LuaScript.hpp"
#include "components/NetworkEntity.hpp"
#include "components/SoundRequest.hpp"
#include "components/Transform.hpp"
#include "components/Velocity.hpp"
#include "components/WaveMovement.hpp"
#include "components/dropPowerUp.hpp"
#include "components/powerUp.hpp"

#include "metrics/Metrics.hpp"
#include "protocol/PacketReader.hpp"
#include "systems/AnimationCleanSystem.hpp"
#include "systems/AnimationSystem.hpp"
#include "systems/AnimatorSystem.hpp"
#include "systems/AttachmentSystem.hpp"
#include "systems/AudioSystem.hpp"
#include "systems/ColliderSystem.hpp"
#include "systems/CollisionFlashSystem.hpp"
#include "systems/DeadSystem.hpp"
#include "systems/DebugCollisionSystem.hpp"
#include "systems/HudSystem.hpp"
#include "systems/NetworkHudSystem.hpp"
#include "systems/InputSystem.hpp"
#include "systems/MovementSystem.hpp"
#include "systems/ParallaxSystem.hpp"
#include "systems/PlayerSystem.hpp"
#include "systems/RenderConcept.hpp"
#include "systems/RenderSystem.hpp"

#include "Protocol/EntityPayload.hpp"
#include "Protocol/SnapshotPayload.hpp"
#include "factoryDataDriven/factoryDataDriven.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <variant>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace rtype::client {
Game::Game(std::string host, std::string port)
    : _window({1280.f, 720.f}, "R-Type"), _adapter(_window),
      _chat(_window, _adapter), _dependency{_window, _networkClient, *this},
      _sceneManager(_dependency, std::in_place_type<IntroScene>),
      _host(std::move(host)), _port(std::move(port)) {}

void Game::onLoad(rtk::EngineContext &context) {
  registerComponents(context);
  createBindings();

  registerBuilders(context);
  loadEntityBluePrint(context);

  _fontRtype = _window.loadFont("./assets/font/r-type.otf", 40);
  _chat.setFont(_fontRtype);

  _networkClient.connect(_host, _port);

  _shootSoundId = _adapterAudioManager.addSound("assets/shoot.wav");

  subscribeEvents(context, _shootSoundId);
  addSystems(context);

  _sceneManager.start(context);
}

void Game::loadEntityBluePrint(rtk::EngineContext &context) {
  context._entityFactory.loadBlueprintManifest("assets/blueprints.json");

  const auto &factory = context._entityFactory;
  _playerArchetypes[0] = factory.getArchetypeID("player_blue");
  _playerArchetypes[1] = factory.getArchetypeID("player_green");
  _playerArchetypes[2] = factory.getArchetypeID("player_pink");
  _playerArchetypes[3] = factory.getArchetypeID("player_red");
}

void Game::registerBuilders(rtk::EngineContext &context) {
  context._entityFactory.registerBuilder("transform",
                                         component::transformFromJson);
  context._entityFactory.registerBuilder("Transform",
                                         component::transformFromJson);
  context._entityFactory.registerBuilder("velocity",
                                         component::velocityFromJson);
  context._entityFactory.registerBuilder("Velocity",
                                         component::velocityFromJson);
  context._entityFactory.registerBuilder("aabbCollider",
                                         component::aabbColliderFromJson);
  context._entityFactory.registerBuilder("AABBCollider",
                                         component::aabbColliderFromJson);
  context._entityFactory.registerBuilder("collider",
                                         component::aabbColliderFromJson);
  context._entityFactory.registerBuilder("Collider",
                                         component::aabbColliderFromJson);
  context._entityFactory.registerBuilder("animation",
                                         component::animationFromJson);
  context._entityFactory.registerBuilder("Animation",
                                         component::animationFromJson);
  context._entityFactory.registerBuilder("dead", component::deadFromJson);
  context._entityFactory.registerBuilder("Dead", component::deadFromJson);
  context._entityFactory.registerBuilder("networkEntity",
                                         component::networkEntityFromJson);
  context._entityFactory.registerBuilder("NetworkEntity",
                                         component::networkEntityFromJson);
  context._entityFactory.registerBuilder("soundRequest",
                                         component::soundRequestFromJson);
  context._entityFactory.registerBuilder("SoundRequest",
                                         component::soundRequestFromJson);
  context._entityFactory.registerBuilder("bullet", component::bulletFromJson);
  context._entityFactory.registerBuilder("Bullet", component::bulletFromJson);
  context._entityFactory.registerBuilder("controllable",
                                         component::controllableFromJson);
  context._entityFactory.registerBuilder("Controllable",
                                         component::controllableFromJson);
  context._entityFactory.registerBuilder("enemy", component::enemyFromJson);
  context._entityFactory.registerBuilder("Enemy", component::enemyFromJson);
  context._entityFactory.registerBuilder("enemyEvent",
                                         component::enemyEventFromJson);
  context._entityFactory.registerBuilder("EnemyEvent",
                                         component::enemyEventFromJson);
  context._entityFactory.registerBuilder(
      "enemyEscapedEvent", rtype::events::enemyEscapedEventFromJson);
  context._entityFactory.registerBuilder(
      "EnemyEscapedEvent", rtype::events::enemyEscapedEventFromJson);
  context._entityFactory.registerBuilder("Animator",
                                         component::animatorFromJson);
  context._entityFactory.registerBuilder("animator",
                                         component::animatorFromJson);
  context._entityFactory.registerBuilder("Parallax",
                                         component::parallaxFromJson);
  context._entityFactory.registerBuilder("parallax",
                                         component::parallaxFromJson);
  context._entityFactory.registerBuilder("waveMovement",
                                         component::waveMovementFromJson);
  context._entityFactory.registerBuilder("WaveMovement",
                                         component::waveMovementFromJson);
  context._entityFactory.registerBuilder("luaScript",
                                         component::luaScriptFromJson);
  context._entityFactory.registerBuilder("LuaScript",
                                         component::luaScriptFromJson);

  context._entityFactory.registerBuilder(
      "sprite", [this](rtk::ecs::Registry &reg, std::size_t entity,
                       const nlohmann::json &data) {
        rtk::ecs::concepts::SpriteData sprite{};
        const std::string texPath = data.value("texture", "default.png");

        sprite.textureId = _window.loadTexture(texPath.data()).getHandle();
        sprite.size = {data["size"].value("w", 0.f),
                       data["size"].value("h", 0.f)};
        sprite.origin = {data["origin"].value("x", 0.f),
                         data["origin"].value("y", 0.f)};
        sprite.textureRect = {data["textureRect"].value<uint16_t>("x", 0),
                              data["textureRect"].value<uint16_t>("y", 0),
                              data["textureRect"].value<uint16_t>("w", 0),
                              data["textureRect"].value<uint16_t>("h", 0)};
        sprite.color = {data["color"].value<uint8_t>("r", 255),
                        data["color"].value<uint8_t>("g", 255),
                        data["color"].value<uint8_t>("b", 255),
                        data["color"].value<uint8_t>("a", 255)};
        sprite.layer = data.value("layer", -1);
        sprite.flags = data.value("flags", 0);

        reg.get_components<rtk::ecs::concepts::SpriteData>().insert_at(entity,
                                                                       sprite);
      });
  context._entityFactory.registerBuilder("attachedTo",
                                         component::attachedToFromJson);
  context._entityFactory.registerBuilder("AttachedTo",
                                         component::attachedToFromJson);
  context._entityFactory.registerBuilder("destroyableTile",
                                         component::destroyableTileFromJson);
  context._entityFactory.registerBuilder("DestroyableTile",
                                         component::destroyableTileFromJson);
  context._entityFactory.registerBuilder("health", component::healthFromJson);
  context._entityFactory.registerBuilder("Health", component::healthFromJson);
  context._entityFactory.registerBuilder("powerUp", component::powerupFromJson);
  context._entityFactory.registerBuilder("powerup", component::powerupFromJson);
  context._entityFactory.registerBuilder("PowerUp", component::powerupFromJson);
  context._entityFactory.registerBuilder("Powerup", component::powerupFromJson);
  context._entityFactory.registerBuilder("dropPowerup",
                                         component::dropPowerupFromJson);
  context._entityFactory.registerBuilder("DropPowerup",
                                         component::dropPowerupFromJson);
  context._entityFactory.registerBuilder("DropPowerUp",
                                         component::dropPowerupFromJson);
  context._entityFactory.registerBuilder("droppowerup",
                                         component::dropPowerupFromJson);
  context._entityFactory.registerBuilder("destroyAnimEnd",
                                         component::destroyAnimEndFromJson);
  context._entityFactory.registerBuilder("deathEffect",
                                         component::deathEffectFromJson);
}

void Game::registerComponents(rtk::EngineContext &context) {
  context.registry.register_component<component::Transform>();
  context.registry.register_component<rtk::ecs::concepts::SpriteData>();
  context.registry.register_component<component::AABBCollider>();
  context.registry.register_component<component::Animation>();
  context.registry.register_component<component::Controllable>();
  context.registry.register_component<component::Velocity>();
  context.registry.register_component<component::Bullet>();
  context.registry.register_component<component::Enemy>();
  context.registry.register_component<component::Dead>();
  context.registry.register_component<component::NetworkEntity>();
  context.registry.register_component<component::SoundRequest>();
  context.registry.register_component<component::animator>();
  context.registry.register_component<rtype::events::EnemyEscapedEvent>();
  context.registry.register_component<component::Parallax>();
  context.registry.register_component<component::AttachedTo>();
  context.registry.register_component<component::Health>();
  context.registry.register_component<component::DestroyableTile>();
  context.registry.register_component<component::WaveMovement>();
  context.registry.register_component<component::LuaScript>();
  context.registry.register_component<component::powerUp>();
  context.registry.register_component<component::DropPowerUp>();
  context.registry.register_component<component::DestroyAnimEnd>();
  context.registry.register_component<component::DeathEffect>();
}

void Game::createBindings() {
  _inputMapping = rtk::Input::InputMapping();

  _inputMapping.bindKey(0, rtk::ecs::concepts::Key::Up);
  _inputMapping.bindKey(0, rtk::ecs::concepts::Key::Z);
  _inputMapping.bindGamepad(0, rtk::ecs::concepts::GamepadButton::DpadUp);
  _inputMapping.bindGamepadAxis(0, rtk::ecs::concepts::GamepadAxis::LeftY,
                                -0.30f);

  _inputMapping.bindKey(1, rtk::ecs::concepts::Key::Down);
  _inputMapping.bindKey(1, rtk::ecs::concepts::Key::S);
  _inputMapping.bindGamepad(1, rtk::ecs::concepts::GamepadButton::DpadDown);
  _inputMapping.bindGamepadAxis(1, rtk::ecs::concepts::GamepadAxis::LeftY,
                                0.30f);

  _inputMapping.bindKey(2, rtk::ecs::concepts::Key::Left);
  _inputMapping.bindKey(2, rtk::ecs::concepts::Key::Q);
  _inputMapping.bindGamepad(2, rtk::ecs::concepts::GamepadButton::DpadLeft);
  _inputMapping.bindGamepadAxis(2, rtk::ecs::concepts::GamepadAxis::LeftX,
                                -0.30f);

  _inputMapping.bindKey(3, rtk::ecs::concepts::Key::Right);
  _inputMapping.bindKey(3, rtk::ecs::concepts::Key::D);
  _inputMapping.bindGamepad(3, rtk::ecs::concepts::GamepadButton::DpadRight);
  _inputMapping.bindGamepadAxis(3, rtk::ecs::concepts::GamepadAxis::LeftX,
                                0.30f);

  _inputMapping.bindKey(4, rtk::ecs::concepts::Key::Space);
  _inputMapping.bindGamepadAxis(4, rtk::ecs::concepts::GamepadAxis::R2, 0.30f);

  _inputMapping.bindKey(5, rtk::ecs::concepts::Key::Enter);
  _inputMapping.bindKey(5, rtk::ecs::concepts::Key::Space);
  _inputMapping.bindGamepad(5, rtk::ecs::concepts::GamepadButton::Cross);

  _inputMapping.bindKey(6, rtk::ecs::concepts::Key::F3);
  _inputMapping.bindGamepad(6, rtk::ecs::concepts::GamepadButton::Select);

  _inputMapping.bindKey(7, rtk::ecs::concepts::Key::H);
  _inputMapping.bindKey(8, rtk::ecs::concepts::Key::R);
  _inputMapping.bindKey(9, rtk::ecs::concepts::Key::Escape);
  _inputMapping.bindKey(10, rtk::ecs::concepts::Key::E);
  _inputMapping.bindKey(10, rtk::ecs::concepts::Key::Tab);
  _inputMapping.bindGamepad(10, rtk::ecs::concepts::GamepadButton::Y);
}

void Game::subscribeEvents(rtk::EngineContext &context, uint32_t shootSoundId) {
  context.eventBus.subscribe<rtype::events::SendPacketEvent>(
      [this](const rtype::events::SendPacketEvent &event) {
        _networkClient.send(event.data);
      });

  context.eventBus.subscribe<rtype::events::PlayerShotEvent>(
      [this, shootSoundId](const rtype::events::PlayerShotEvent &) {
        _adapterAudioManager.playSound(shootSoundId);
        if (_hudSystem) {
          _hudSystem->incrementShotCount();
        }
      });

  context.eventBus.subscribe<rtype::events::BeamChargeEvent>(
      [this](const rtype::events::BeamChargeEvent &event) {
        if (_hudSystem) {
          _hudSystem->setBeamCharge(event.chargeRatio, event.chargeLevel,
                                    event.isCharging);
        }
      });

  context.eventBus.subscribe<rtype::events::EnemyEscapedEvent>(
      [this, shootSoundId](const rtype::events::EnemyEscapedEvent &) {
        _adapterAudioManager.playSound(shootSoundId);
      });

  context.eventBus.subscribe<rtype::events::QuitGameEvent>(
      [this](const rtype::events::QuitGameEvent &) {
        _shouldQuit = true;
        _inputState.closeRequested = true;
      });

  context.eventBus.subscribe<rtype::events::PlayMusicEvent>(
      [this](const rtype::events::PlayMusicEvent &event) {
        playMusic(event.path, event.loop);
      });

  context.eventBus.subscribe<rtype::events::StopMusicEvent>(
      [this](const rtype::events::StopMusicEvent &) {
        stopMusic();
      });

  context.eventBus.subscribe<
      rtk::collision::Collision>([this, &context](
                                     const rtk::collision::Collision &event) {
    auto *colA = context.registry.get_components<component::AABBCollider>().get(
        event.firstEntity);
    auto *colB = context.registry.get_components<component::AABBCollider>().get(
        event.secondEntity);
    if (!colA || !colB)
      return;

    const std::uint32_t collisionPair = colA->layer | colB->layer;
    const std::uint32_t playerVsPowerup =
        Collider::CollisionLayer::Player | Collider::CollisionLayer::PowerUp;

    if (collisionPair == playerVsPowerup) {
      const std::size_t playerEnt =
          (colA->layer == Collider::CollisionLayer::Player)
              ? event.firstEntity
              : event.secondEntity;
      const std::size_t powerupEnt =
          (colA->layer == Collider::CollisionLayer::PowerUp)
              ? event.firstEntity
              : event.secondEntity;

      auto &deads = context.registry.get_components<component::Dead>();
      if (deads.get(powerupEnt) != nullptr)
        return;
      deads.insert_at(powerupEnt, {});

      auto *powerupComp =
          context.registry.get_components<component::powerUp>().get(powerupEnt);
      if (powerupComp) {
        if (powerupComp->type == component::powerUpType::Speed) {
          auto *speedComp =
              context.registry.get_components<component::Controllable>().get(
                  playerEnt);
          constexpr float MAX_SPEED = 440.0f;
          if (speedComp) {
            speedComp->speed =
                std::min(speedComp->speed + powerupComp->value, MAX_SPEED);
          }
          if (_hudSystem) {
            _hudSystem->addSpeedBonus();
          }
        } else if (powerupComp->type == component::powerUpType::WeaponSpread) {
          const float duration =
              (powerupComp->value > 0.0f) ? powerupComp->value : 5.0f;
          auto *ctrl =
              context.registry.get_components<component::Controllable>().get(
                  playerEnt);
          if (ctrl) {
            ctrl->weaponType = 1;
            ctrl->weaponTimer = duration;
            ctrl->weaponMaxDuration = duration;
          }
          if (_hudSystem) {
            _hudSystem->setWeaponWithTimer(1, duration);
          }
        } else if (powerupComp->type == component::powerUpType::WeaponPlasma) {
          const float duration =
              (powerupComp->value > 0.0f) ? powerupComp->value : 5.0f;
          auto *ctrl =
              context.registry.get_components<component::Controllable>().get(
                  playerEnt);
          if (ctrl) {
            ctrl->weaponType = 2;
            ctrl->weaponTimer = duration;
            ctrl->weaponMaxDuration = duration;
          }
          if (_hudSystem) {
            _hudSystem->setWeaponWithTimer(2, duration);
          }
        }
      }
    }
  });
}

void Game::addSystems(rtk::EngineContext &context) {
  context.scheduler.add<rtk::systems::AnimationSystem>(rtk::ecs::Order::Render);
  context.scheduler.add<rtk::systems::PlayerSystem>(
      rtk::ecs::Order::Update, std::ref(_inputState),
      std::ref(context.eventBus), std::ref(context._entityFactory),
      std::ref(_networkClient), std::ref(_prediction));
  context.scheduler.add<rtk::systems::InputSystem<RtkWindowAdapter>>(
      rtk::ecs::Order::Input, std::ref(_adapter), std::ref(_inputState),
      std::ref(_inputMapping), false);
  context.scheduler.add<rtk::systems::MovementSystem>(rtk::ecs::Order::Physics);
  _collisionFlashSystem =
      &context.scheduler.add<rtk::systems::CollisionFlashSystem>(
          rtk::ecs::Order::Render, _collisionBuffer);
  context.scheduler.add<rtk::systems::RenderSystem<RtkWindowAdapter>>(
      rtk::ecs::Order::Render, std::ref(_adapter));
  context.scheduler.add<rtk::systems::DebugCollisionSystem>(
      rtk::ecs::Order::Physics, _collisionBuffer, _window, _inputState);
  context.scheduler.add<rtk::systems::ColliderSystem>(
      rtk::ecs::Order::Physics, _collisionBuffer, 64.f, context.eventBus);
  context.scheduler.add<rtk::systems::AudioSystem<RtkAudioAdapter>>(
      rtk::ecs::Order::Update, std::ref(_adapterAudioManager));
  context.scheduler.add<rtk::systems::AnimatorSystem>(rtk::ecs::Order::Update);
  context.scheduler.add<rtk::systems::ParallaxSystem>(rtk::ecs::Order::Update);
  context.scheduler.add<rtk::systems::AttachmentSystem>(
      rtk::ecs::Order::Update);
  context.scheduler.add<rtk::systems::AnimationCleanSystem>(
      rtk::ecs::Order::Update);
  context.scheduler.add<rtk::systems::DeathSystem>(rtk::ecs::Order::Update);
  _hudSystem = &context.scheduler.add<rtk::systems::HudSystem>(
      rtk::ecs::Order::Update, std::ref(_window), std::ref(_inputState),
      std::ref(context.eventBus), _fontRtype);
  context.scheduler.add<rtk::systems::NetworkHudSystem>(
      rtk::ecs::Order::Update, std::ref(_window), std::ref(_inputState),
      std::cref(_networkStats), std::cref(_networkDiagnostics),
      std::cref(_prediction), _fontRtype);
}

bool Game::shouldContinue() const {
  return !_inputState.closeRequested && !_shouldQuit;
}

static double diagnosticsNow() {
  return std::chrono::duration<double>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

void Game::onUpdate(rtk::EngineContext &context, float dt) {
  updateNetworkStats();
  if (_networkClient.isConnected())
    _networkDiagnostics.onInput(_networkClient.inputTick(), diagnosticsNow());
  NetworkEvent netEvent;
  while (_networkClient.pollEvent(netEvent)) {
    if (netEvent.type == NetworkEventType::Connected) {
      context.eventBus.publish(rtype::events::ConnectedToServerEvent{});
    } else if (netEvent.type == NetworkEventType::Rejected) {
      context.eventBus.publish(rtype::events::ConnectionRejectedEvent{});
    } else if (netEvent.type == NetworkEventType::Disconnected) {
      LOG_ERROR("Disconnected from the server");
      _shouldQuit = true;
    } else if (netEvent.type == NetworkEventType::Message) {
      switch (netEvent.messageType) {
      case rtype::protocol::MessageType::RoomList: {
        PacketReader reader(netEvent.payload);
        protocol::RoomListPayload payload{};
        if (!protocol::readRoomList(reader, payload)) {
          break;
        }
        if (!reader.isEnd()) {
          break;
        }
        _availableRooms = payload.rooms;
        break;
      }
      case rtype::protocol::MessageType::RoomCreated: {
        PacketReader reader(netEvent.payload);
        protocol::RoomResultPayload payload{};
        if (!protocol::readRoomResult(reader, payload)) {
          break;
        }
        if (!reader.isEnd()) {
          break;
        }
        if (payload.result != protocol::RoomResult::Success) {
          break;
        }
        _currentRoomId = payload.roomId.get();
        _chat.clearHistory();
        _roomPlayers.clear();
        _localReady = false;
        _readyRequestPending = false;
        _gameStarting = false;
        break;
      }

      case rtype::protocol::MessageType::RoomJoined: {
        PacketReader reader(netEvent.payload);
        protocol::RoomResultPayload payload{};
        if (!protocol::readRoomResult(reader, payload)) {
          break;
        }
        if (!reader.isEnd()) {
          break;
        }
        if (payload.result != protocol::RoomResult::Success) {
          break;
        }
        _currentRoomId = payload.roomId.get();
        _chat.clearHistory();
        _roomPlayers.clear();
        _localReady = false;
        _readyRequestPending = false;
        _gameStarting = false;
        break;
      }
      case rtype::protocol::MessageType::RoomLeft: {
        PacketReader reader(netEvent.payload);
        protocol::RoomResultPayload payload{};
        if (!protocol::readRoomResult(reader, payload)) {
          break;
        }
        if (!reader.isEnd()) {
          break;
        }
        if (payload.result != protocol::RoomResult::Success) {
          break;
        }
        _currentRoomId.reset();
        _chat.clearHistory();
        _roomPlayers.clear();
        _localReady = false;
        _readyRequestPending = false;
        _gameStarting = false;
        clearNetworkEntities(context);
        _networkClient.sendListRoom();
        break;
      }
      case rtype::protocol::MessageType::RoomStateUpdate: {
        PacketReader reader(netEvent.payload);
        protocol::RoomStateUpdatePayload payload{};
        if (!protocol::readRoomStateUpdate(reader, payload))
          break;
        if (!reader.isEnd())
          break;
        if (!_currentRoomId.has_value() || *_currentRoomId != payload.roomId)
          break;
        _roomPlayers = std::move(payload.players);
        const auto localConnectionId = _networkClient.getConnectionId();
        bool localPlayerFound = false;
        for (const auto &player : _roomPlayers) {
          if (player.connectionId != localConnectionId)
            continue;
          _localReady = player.ready;
          _readyRequestPending = false;
          localPlayerFound = true;
          break;
        }
        if (!localPlayerFound) {
          _localReady = false;
        }
        bool anyPlaying = false;
        for (const auto &player : _roomPlayers) {
          if (player.ready)
            anyPlaying = true;
        }
        if (!anyPlaying) {
          _gameStarting = false;
        }
        break;
      }
      case rtype::protocol::MessageType::GameStarting: {
        PacketReader reader(netEvent.payload);
        protocol::GameStartingPayload payload{};
        if (!protocol::readPayload(reader, payload))
          break;
        if (!reader.isEnd())
          break;
        if (!_currentRoomId.has_value() ||
            *_currentRoomId != payload.roomId.get())
          break;
        clearNetworkEntities(context);
        _gameStarting = true;
        _readyRequestPending = false;
        break;
      }
      case rtype::protocol::MessageType::ChatMessage: {
        PacketReader reader(netEvent.payload);
        protocol::ChatMessagePayload payload{};
        if (!protocol::readChatMessage(reader, payload))
          break;
        if (!_currentRoomId.has_value() || *_currentRoomId != payload.roomId)
          break;
        _chat.addMessage(payload.senderId, payload.text,
                         payload.senderId == _networkClient.getConnectionId());
        break;
      }
      default:
        break;
      }
    }
  }

  rtype::client::ReplicationEvent repEvent;
  while (_networkClient.pollReplicationEvent(repEvent)) {
    if (std::holds_alternative<protocol::CreateEntityPayload>(
            repEvent.payload)) {
      const auto &createPayload =
          std::get<protocol::CreateEntityPayload>(repEvent.payload);
      _pendingEntityQueries.erase(createPayload.networkId.get());
      _destroyedEntities.erase(createPayload.networkId.get());
      _networkDiagnostics.onEntityCreated(createPayload.networkId.get(),
                                          diagnosticsNow());

      bool alreadyExists = false;
      auto viewNet = context.registry.view<component::NetworkEntity>();
      for (auto e : viewNet) {
        auto *netEnt =
            context.registry.get_components<component::NetworkEntity>().get(e);
        if (netEnt && netEnt->id == createPayload.networkId.get()) {
          alreadyExists = true;
          break;
        }
      }
      if (alreadyExists)
        continue;

      if (!context._entityFactory.hasArchetype(createPayload.entityType.get()))
        continue;

      auto entity = context._entityFactory.createNetwork(
          context.registry, createPayload.entityType.get(),
          createPayload.networkId.get());
      _networkEntities[createPayload.networkId.get()] = entity;

      uint32_t type = createPayload.entityType.get();
      if (type == context._entityFactory.getArchetypeID("player") ||
          type == context._entityFactory.getArchetypeID("player_red") ||
          type == context._entityFactory.getArchetypeID("player_green") ||
          type == context._entityFactory.getArchetypeID("player_blue") ||
          type == context._entityFactory.getArchetypeID("player_pink")) {
        std::size_t thrusterEntity = context._entityFactory.createFromBlueprint(
            context.registry, "thruster");
        auto *attached =
            context.registry.get_components<component::AttachedTo>().get(
                thrusterEntity);
        if (attached)
          attached->targetEntity = entity;
      }
      auto *transform =
          context.registry.get_components<component::Transform>().get(entity);
      if (transform) {
        transform->position.x =
            protocol::decodePosition(createPayload.positionX);
        transform->position.y =
            protocol::decodePosition(createPayload.positionY);
      }
      if (protocol::decodePosition(createPayload.positionY) < 50.0f) {
        auto *sprite =
            context.registry.get_components<rtk::ecs::concepts::SpriteData>()
                .get(entity);
        if (sprite) {
          sprite->flags = 1 << 1;
        }
      }

      if (transform &&
          (type == context._entityFactory.getArchetypeID("bullet_plasma") ||
           (type == context._entityFactory.getArchetypeID("bullet") &&
            std::abs(transform->rotation) > 0.1f))) {
        const std::uint8_t wType =
            (type == context._entityFactory.getArchetypeID("bullet_plasma"))
                ? 2
                : 1;
        for (auto pEnt : context.registry.view<component::Controllable>()) {
          auto *pTrans =
              context.registry.get_components<component::Transform>().get(pEnt);
          if (pTrans &&
              std::abs(transform->position.y - pTrans->position.y) < 40.0f &&
              std::abs(transform->position.x - pTrans->position.x) < 100.0f) {
            auto *ctrl =
                context.registry.get_components<component::Controllable>().get(
                    pEnt);
            if (ctrl) {
              ctrl->weaponType = wType;
              if (ctrl->weaponTimer <= 0.0f) {
                ctrl->weaponTimer = 5.0f;
                ctrl->weaponMaxDuration = 5.0f;
              }
            }
            if (_hudSystem && (_hudSystem->getWeapon() != wType ||
                               _hudSystem->getWeaponTimer() <= 0.0f)) {
              _hudSystem->setWeaponWithTimer(
                  wType, (ctrl && ctrl->weaponTimer > 0.0f) ? ctrl->weaponTimer
                                                            : 5.0f);
            }
          }
        }
      }
    }

    else if (std::holds_alternative<protocol::DestroyEntityPayload>(
                 repEvent.payload)) {
      const auto &destroyPayload =
          std::get<protocol::DestroyEntityPayload>(repEvent.payload);
      _pendingEntityQueries.erase(destroyPayload.networkId.get());
      _networkEntities.erase(destroyPayload.networkId.get());
      _destroyedEntities.insert(destroyPayload.networkId.get());

      if (destroyPayload.reason == protocol::DestroyReason::GameEnded &&
          _gameStarting) {
        LOG_INFO("Game ended");
        _gameStarting = false;
        clearNetworkEntities(context);
      }

      auto view = context.registry.view<component::NetworkEntity>();
      for (auto e : view) {
        auto *netEnt =
            context.registry.get_components<component::NetworkEntity>().get(e);
        if (netEnt && netEnt->id == destroyPayload.networkId.get()) {
          auto *powerupComp =
              context.registry.get_components<component::powerUp>().get(e);
          auto &deads = context.registry.get_components<component::Dead>();

          if (powerupComp && deads.get(e) == nullptr) {
            for (auto pEnt : context.registry.view<component::Controllable>()) {
              auto *ctrl =
                  context.registry.get_components<component::Controllable>()
                      .get(pEnt);
              if (powerupComp->type == component::powerUpType::Speed) {
                constexpr float MAX_SPEED = 440.0f;
                if (ctrl) {
                  ctrl->speed =
                      std::min(ctrl->speed + powerupComp->value, MAX_SPEED);
                }
                if (_hudSystem) {
                  _hudSystem->addSpeedBonus();
                }
              } else if (powerupComp->type ==
                         component::powerUpType::WeaponSpread) {
                const float duration =
                    (powerupComp->value > 0.0f) ? powerupComp->value : 5.0f;
                if (ctrl) {
                  ctrl->weaponType = 1;
                  ctrl->weaponTimer = duration;
                  ctrl->weaponMaxDuration = duration;
                }
                if (_hudSystem) {
                  _hudSystem->setWeaponWithTimer(1, duration);
                }
              } else if (powerupComp->type ==
                         component::powerUpType::WeaponPlasma) {
                const float duration =
                    (powerupComp->value > 0.0f) ? powerupComp->value : 5.0f;
                if (ctrl) {
                  ctrl->weaponType = 2;
                  ctrl->weaponTimer = duration;
                  ctrl->weaponMaxDuration = duration;
                }
                if (_hudSystem) {
                  _hudSystem->setWeaponWithTimer(2, duration);
                }
              }
            }
          }
          if (destroyPayload.reason == protocol::DestroyReason::Collision &&
              _collisionFlashSystem) {
            auto *bullet =
                context.registry.get_components<component::Bullet>().get(e);
            auto *bulletTrans =
                context.registry.get_components<component::Transform>().get(e);
            if (bullet && bulletTrans) {
              auto &transforms =
                  context.registry.get_components<component::Transform>();
              auto viewEnemies =
                  context.registry
                      .view<component::Enemy, component::Transform>();
              float explosionX = bulletTrans->position.x;
              float explosionY = bulletTrans->position.y;
              if (explosionX > -50.0f && explosionX < 1330.0f &&
                  explosionY > -50.0f && explosionY < 770.0f) {
                auto explosion = context._entityFactory.createFromBlueprint(
                    context.registry, "explosion");
                auto *explosionTrans =
                    context.registry.get_components<component::Transform>().get(
                        explosion);
                explosionTrans->position.x = explosionX;
                explosionTrans->position.y = explosionY;
              }
              std::size_t closestEnemy = 0;
              float closestDist = 120.0f;
              for (auto enemyEnt : viewEnemies) {
                auto *enemyTrans = transforms.get(enemyEnt);
                if (!enemyTrans)
                  continue;
                float dx = enemyTrans->position.x - bulletTrans->position.x;
                float dy =
                    std::abs(enemyTrans->position.y - bulletTrans->position.y);
                if (dx >= -40.0f && dx <= 120.0f && dy <= 60.0f) {
                  float dist = std::hypot(dx, dy);
                  if (dist < closestDist) {
                    closestDist = dist;
                    closestEnemy = enemyEnt;
                  }
                }
              }
              if (closestEnemy != 0) {
                _collisionFlashSystem->triggerFlash(context.registry,
                                                    closestEnemy);
              }
            }
          }
          auto *deathEffect =
              context.registry.get_components<component::DeathEffect>().get(e);
          auto *transform =
              context.registry.get_components<component::Transform>().get(e);
          if (transform && deathEffect) {
            auto effectDeath = context._entityFactory.createFromBlueprint(
                context.registry, deathEffect->blueprintName);
            auto *effectTransform =
                context.registry.get_components<component::Transform>().get(
                    effectDeath);
            effectTransform->position.x = transform->position.x;
            effectTransform->position.y = transform->position.y;
          }
          context.registry.remove_entity_from_all_pools(e);
          context.registry.kill_entity(e);
          break;
        }
      }
    }

    else if (std::holds_alternative<protocol::SnapshotPayload>(
                 repEvent.payload)) {
      const auto &snapshotPayload =
          std::get<protocol::SnapshotPayload>(repEvent.payload);

      auto view = context.registry
                      .view<component::NetworkEntity, component::Transform>();
      auto &transforms =
          context.registry.get_components<component::Transform>();
      auto &networkEntities =
          context.registry.get_components<component::NetworkEntity>();
      auto &animators = context.registry.get_components<component::animator>();
      auto &velocities = context.registry.get_components<component::Velocity>();

      _prediction.setOwnNetworkId(snapshotPayload.ownNetworkId.get());
      _networkDiagnostics.onSnapshot(snapshotPayload, diagnosticsNow());

      for (const auto &netData : snapshotPayload.entities) {
        bool found = false;
        for (auto e : view) {
          auto *netEnt = networkEntities.get(e);
          if (netEnt && netEnt->id == netData.networkId.get()) {
            auto *transform = transforms.get(e);
            if (transform && netData.networkId.get() ==
                                 snapshotPayload.ownNetworkId.get()) {
              // The ship of the local player is predicted: the Snapshot only
              // corrects the prediction. Its animation follows the held keys
              // (PlayerSystem).
              const auto reconciliations = _prediction.stats().reconciliations;
              _prediction.reconcile(
                  protocol::decodePosition(netData.positionX),
                  protocol::decodePosition(netData.positionY),
                  snapshotPayload.ackedInputTick.get());
              if (_prediction.stats().reconciliations > reconciliations)
                _networkDiagnostics.onPredictionGap(
                    _prediction.stats().lastError, diagnosticsNow());
              if (_prediction.isActive()) {
                transform->position.x = _prediction.x();
                transform->position.y = _prediction.y();
              }
            } else if (transform) {
              float deltaY = protocol::decodePosition(netData.positionY) -
                             transform->position.y;
              auto *anim = animators.get(e);
              if (anim) {
                if (deltaY < -0.5f) {
                  anim->currentState = component::animState::UP;
                } else if (deltaY > 0.5f) {
                  anim->currentState = component::animState::DOWN;
                } else {
                  anim->currentState = component::animState::IDLE;
                }
              }
              auto *vel = velocities.get(e);
              if (vel) {
                vel->y = deltaY;
              }
              transform->position.x =
                  protocol::decodePosition(netData.positionX);
              transform->position.y =
                  protocol::decodePosition(netData.positionY);
            }
            found = true;
            break;
          }
        }
        if (!found) {
          if (!_destroyedEntities.contains(netData.networkId.get()) &&
              !_pendingEntityQueries.contains(netData.networkId.get())) {
            _pendingEntityQueries.insert(netData.networkId.get());
            _networkDiagnostics.onUnknownEntity(netData.networkId.get(),
                                                diagnosticsNow());
            _networkClient.sendQueryEntity(netData.networkId.get());
          }
        }
      }
    }
  }

  if (_hudSystem) {
    _hudSystem->setInGameScene(_sceneManager.get<GameScene>() != nullptr);
  }

  updateChat(context, dt);

  const bool escapePressed =
      rtype::Input::hasAction(_inputState.actionMask,
                              rtype::Action::LeaveRoom) &&
      !rtype::Input::hasAction(_previousActionMask, rtype::Action::LeaveRoom);
  _previousActionMask = _inputState.actionMask;
  if (escapePressed && _sceneManager.get<RoomLobbyScene>() == nullptr &&
      _sceneManager.get<SettingScene>() == nullptr &&
      _sceneManager.get<GameOverScene>() == nullptr &&
      _sceneManager.get<GameScene>() == nullptr)
    _shouldQuit = true;

  const bool mPressed = isKeyPressed(rtk::ecs::concepts::Key::M);
  if (mPressed && !_previousMuteKey) {
    const bool muted = toggleMute();
    LOG_INFO(std::string("Audio output ") + (muted ? "MUTED" : "UNMUTED"));
  }
  _previousMuteKey = mPressed;

  _sceneManager.update(context, _inputState, dt);
}

void Game::updateChat(rtk::EngineContext &context, float dt) {
  const bool inLobby = _sceneManager.get<RoomLobbyScene>() != nullptr;
  const bool inGame = _sceneManager.get<GameScene>() != nullptr;
  const bool available = _currentRoomId.has_value() && (inLobby || inGame);

  const auto sent = _chat.update(context.registry, dt, available, inLobby);
  if (sent)
    _networkClient.sendChat(*sent);
  _inputState.textInputActive = _chat.capturesKeyboard();
}

void Game::clearNetworkEntities(rtk::EngineContext &context) {
  std::vector<std::size_t> entities;
  for (auto entity : context.registry.view<component::NetworkEntity>())
    entities.push_back(entity);

  for (auto entity : context.registry.view<component::AttachedTo>())
    entities.push_back(entity);

  for (auto entity : entities) {
    context.registry.remove_entity_from_all_pools(entity);
    context.registry.kill_entity(entity);
  }
  context.registry.flush();

  _networkEntities.clear();
  _pendingEntityQueries.clear();
  _destroyedEntities.clear();
  _lastServerTick = 0;
  _prediction.reset();
  _networkDiagnostics.reset();
}

void Game::onUnload(rtk::EngineContext &context) {
  _chat.destroyEntities(context.registry);
  for (const auto &[_, entity] : _networkEntities)
    context.registry.kill_entity(entity);
  _networkEntities.clear();
  _sceneManager.stop(context);
  _networkClient.stop();
}

void Game::createReplicatedEntity(
    rtk::EngineContext &context, const protocol::CreateEntityPayload &payload) {
  if (_networkEntities.contains(payload.networkId.get()) ||
      !context._entityFactory.hasArchetype(payload.entityType.get()))
    return;

  const auto entity = context._entityFactory.createNetwork(
      context.registry, payload.entityType.get(), payload.networkId.get());

  auto *transform =
      context.registry.get_components<component::Transform>().get(entity);
  if (!transform) {
    context.registry.kill_entity(entity);
    return;
  }

  transform->position = {protocol::decodePosition(payload.positionX),
                         protocol::decodePosition(payload.positionY)};
  transform->rotation = protocol::decodeRotation(payload.rotation);

  auto *velocity =
      context.registry.get_components<component::Velocity>().get(entity);
  if (velocity && transform->rotation != 0.0f) {
    const float currentSpeed = std::hypot(velocity->x, velocity->y);
    if (currentSpeed > 0.0f) {
      velocity->x = currentSpeed * std::cos(transform->rotation);
      velocity->y = currentSpeed * std::sin(transform->rotation);
    }
  }

  _networkEntities.emplace(payload.networkId.get(), entity);
}

void Game::destroyReplicatedEntity(
    rtk::EngineContext &context,
    const protocol::DestroyEntityPayload &payload) {

  const auto it = _networkEntities.find(payload.networkId.get());

  if (it == _networkEntities.end())
    return;

  const auto entity = it->second;
  auto &deads = context.registry.get_components<component::Dead>();
  auto *powerupComp =
      context.registry.get_components<component::powerUp>().get(entity);

  if (powerupComp && deads.get(entity) == nullptr &&
      payload.reason == protocol::DestroyReason::Collision) {
    for (auto pEnt : context.registry.view<component::Controllable>()) {
      auto *ctrl =
          context.registry.get_components<component::Controllable>().get(pEnt);
      if (powerupComp->type == component::powerUpType::Speed) {
        constexpr float MAX_SPEED = 440.0f;
        if (ctrl) {
          ctrl->speed = std::min(ctrl->speed + powerupComp->value, MAX_SPEED);
        }
        if (_hudSystem) {
          _hudSystem->addSpeedBonus();
        }
      } else if (powerupComp->type == component::powerUpType::WeaponSpread) {
        const float duration =
            (powerupComp->value > 0.0f) ? powerupComp->value : 5.0f;
        if (ctrl) {
          ctrl->weaponType = 1;
          ctrl->weaponTimer = duration;
          ctrl->weaponMaxDuration = duration;
        }
        if (_hudSystem) {
          _hudSystem->setWeaponWithTimer(1, duration);
        }
      } else if (powerupComp->type == component::powerUpType::WeaponPlasma) {
        const float duration =
            (powerupComp->value > 0.0f) ? powerupComp->value : 5.0f;
        if (ctrl) {
          ctrl->weaponType = 2;
          ctrl->weaponTimer = duration;
          ctrl->weaponMaxDuration = duration;
        }
        if (_hudSystem) {
          _hudSystem->setWeaponWithTimer(2, duration);
        }
      }
    }
  }

  context.registry.kill_entity(entity);
  _networkEntities.erase(it);
}

void Game::applySnapshot(rtk::EngineContext &context,
                         const protocol::SnapshotPayload &payload) {
  const auto delta =
      static_cast<std::int32_t>(payload.serverTick.get() - _lastServerTick);

  if (_lastServerTick != 0 && delta < 0)
    return;

  auto &transforms = context.registry.get_components<component::Transform>();
  auto &velocities = context.registry.get_components<component::Velocity>();

  for (const auto &state : payload.entities) {
    const auto it = _networkEntities.find(state.networkId.get());
    if (it == _networkEntities.end())
      continue;

    auto *transform = transforms.get(it->second);
    if (!transform)
      continue;

    const float newX = protocol::decodePosition(state.positionX);
    const float newY = protocol::decodePosition(state.positionY);
    const float newRot = protocol::decodeRotation(state.rotation);

    auto *velocity = velocities.get(it->second);
    if (velocity && newRot != 0.0f) {
      const float currentSpeed = std::hypot(velocity->x, velocity->y);
      if (currentSpeed > 0.0f) {
        velocity->x = currentSpeed * std::cos(newRot);
        velocity->y = currentSpeed * std::sin(newRot);
      }
    }

    transform->position = {newX, newY};
    transform->rotation = newRot;
  }
  _lastServerTick = payload.serverTick.get();
  LOG_INFO("[SNAPSHOT APPLIED]");
}

void Game::handleReplication(rtk::EngineContext &context) {
  ReplicationEvent event;

  while (_networkClient.pollReplicationEvent(event)) {
    std::visit(
        [&](const auto &payload) {
          using T = std::decay_t<decltype(payload)>;
          if constexpr (std::is_same_v<T, protocol::CreateEntityPayload>) {
            createReplicatedEntity(context, payload);
          } else if constexpr (std::is_same_v<T,
                                              protocol::DestroyEntityPayload>) {
            destroyReplicatedEntity(context, payload);
          } else if constexpr (std::is_same_v<T, protocol::SnapshotPayload>) {
            applySnapshot(context, payload);
          }
        },
        event.payload);
  }
}

const ClientNetworkStats &Game::getNetworkStats() const {
  return _networkStats;
}

void Game::updateNetworkStats() {
  const auto now = std::chrono::steady_clock::now();
  const NetworkMetrics metrics = _networkClient.getMetrics();

  _networkStats.pingMs = metrics.rtt;
  _networkStats.smoothPingMs = metrics.smoothRtt;
  _networkStats.jitterMs = metrics.jitter;
  _networkStats.totalRetransmissions = metrics.retransmissions;
  _networkStats.totalDuplicates = metrics.packetsDuplicated;
  _networkStats.totalOutOfOrder = metrics.packetsOut;
  _networkStats.totalBytesReceived = metrics.bytesReceived;
  _networkStats.totalBytesSent = metrics.bytesSent;
  _networkStats.packetLossPercent = metrics.packetLossPercentage;
  _networkStats.totalPacketsReceived = metrics.packetsReceived;
  _networkStats.totalPacketsSent = metrics.packetsSent;
  _networkStats.totalPacketsLost = metrics.packetsLost;

  if (!_networkMetricsInitialized) {
    _previousNetworkMetrics = metrics;
    _lastNetworkMetricsTime = now;
    _networkMetricsInitialized = true;
    return;
  }

  const double elapsed =
      std::chrono::duration<double>(now - _lastNetworkMetricsTime).count();
  if (elapsed < 1.0)
    return;
  const auto previous = _previousNetworkMetrics;

  if (metrics.bytesReceived < previous.bytesReceived ||
      metrics.bytesSent < previous.bytesSent ||
      metrics.packetsReceived < previous.packetsReceived ||
      metrics.packetsSent < previous.packetsSent ||
      metrics.retransmissions < previous.retransmissions) {
    _networkStats.downloadKiBs = 0.0;
    _networkStats.uploadKiBs = 0.0;
    _networkStats.packetsInPerSecond = 0.0;
    _networkStats.packetsOutPerSecond = 0.0;
    _networkStats.retransmissionsPerSecond = 0.0;
    _previousNetworkMetrics = metrics;
    _lastNetworkMetricsTime = now;
    return;
  }

  _networkStats.downloadKiBs =
      static_cast<double>(metrics.bytesReceived - previous.bytesReceived) /
      elapsed / 1024.0;
  _networkStats.uploadKiBs =
      static_cast<double>(metrics.bytesSent - previous.bytesSent) / elapsed /
      1024.0;
  _networkStats.packetsInPerSecond =
      static_cast<double>(metrics.packetsReceived - previous.packetsReceived) /
      elapsed;
  _networkStats.packetsOutPerSecond =
      static_cast<double>(metrics.packetsSent - previous.packetsSent) / elapsed;
  _networkStats.retransmissionsPerSecond =
      static_cast<double>(metrics.retransmissions - previous.retransmissions) /
      elapsed;
  _previousNetworkMetrics = metrics;
  _lastNetworkMetricsTime = now;
}

const std::vector<protocol::RoomInfoPayload> &Game::getAvailableRooms() const {
  return _availableRooms;
}

const std::optional<std::uint32_t> &Game::getCurrentRoomId() const {
  return _currentRoomId;
}

const std::vector<protocol::RoomPlayerState> &Game::getRoomPlayers() const {
  return _roomPlayers;
}

bool Game::isLocalReady() const { return _localReady; }

bool Game::isReadyRequestPending() const { return _readyRequestPending; }

bool Game::isGameStarting() const { return _gameStarting; }

void Game::setGameStarting(bool starting) { _gameStarting = starting; }

bool Game::requestReady(bool ready) {
  if (!_currentRoomId.has_value())
    return false;

  if (_readyRequestPending)
    return false;

  if (_gameStarting)
    return false;

  if (_localReady == ready)
    return false;

  _readyRequestPending = true;
  _networkClient.sendSetReady(ready);
  return true;
}

bool Game::requestLeaveRoom() {
  if (!_currentRoomId.has_value())
    return false;
  if (_gameStarting)
    return false;
  _networkClient.sendLeaveRoom();
  return true;
}

const rtk::Font &Game::getFont() const { return _fontRtype; }
} // namespace rtype::client
