#include "game.hpp"
#include "ClientMetrics.hpp"
#include "EngineContext.hpp"
#include "Logger/Logger.hpp"
#include "NetworkClient.hpp"
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
      _dependency{_window, _networkClient, *this},
      _sceneManager(_dependency, std::in_place_type<IntroScene>),
      _host(std::move(host)), _port(std::move(port)) {}

void Game::onLoad(rtk::EngineContext &context) {
  registerComponents(context);
  createBindings();

  registerBuilders(context);
  loadEntityBluePrint(context);

  _fontRtype = _window.loadFont("./assets/font/r-type.otf", 40);

  _networkClient.connect(_host, _port);

  uint32_t shootSoundId = _adapterAudioManager.addSound("assets/shoot.wav");

  subscribeEvents(context, shootSoundId);
  addSystems(context);

  _sceneManager.start(context);
}

void Game::loadEntityBluePrint(rtk::EngineContext &context) {
  _playerArchetypes[0] = context._entityFactory.loadBlueprintFromPath(
      "player_blue", "assets/descriptionEntity/player_blue.json");
  _playerArchetypes[1] = context._entityFactory.loadBlueprintFromPath(
      "player_green", "assets/descriptionEntity/player_green.json");

  _playerArchetypes[2] = context._entityFactory.loadBlueprintFromPath(
      "player_pink", "assets/descriptionEntity/player_pink.json");

  _playerArchetypes[3] = context._entityFactory.loadBlueprintFromPath(
      "player_red", "assets/descriptionEntity/player_red.json");

  context._entityFactory.loadBlueprintFromPath(
      "player", "assets/descriptionEntity/player.json");

  context._entityFactory.loadBlueprintFromPath(
      "bullet", "assets/descriptionEntity/bullet.json");

  context._entityFactory.loadBlueprintFromPath(
      "enemyBidos", "assets/descriptionEntity/enemyBidos.json");

  context._entityFactory.loadBlueprintFromPath(
      "enemyRedShip", "assets/descriptionEntity/enemyRedShip.json");

  context._entityFactory.loadBlueprintFromPath(
      "gate", "assets/descriptionEntity/gate.json");

  context._entityFactory.loadBlueprintFromPath(
      "ramp", "assets/descriptionEntity/ramp.json");

  context._entityFactory.loadBlueprintFromPath(
      "reactor", "assets/descriptionEntity/reactor.json");

  context._entityFactory.loadBlueprintFromPath(
      "high_ground", "assets/descriptionEntity/high_ground.json");

  context._entityFactory.loadBlueprintFromPath(
      "low_ground", "assets/descriptionEntity/low_ground.json");

  context._entityFactory.loadBlueprintFromPath(
      "medium_ground", "assets/descriptionEntity/medium_ground.json");

  context._entityFactory.loadBlueprintFromPath(
      "enemy_bullet", "assets/descriptionEntity/enemy_bullet.json");

  context._entityFactory.loadBlueprintFromPath(
      "background_stars", "assets/descriptionEntity/background_star.json");
  context._entityFactory.loadBlueprintFromPath(
      "background_fast_stars",
      "assets/descriptionEntity/background_fast_star.json");
  context._entityFactory.loadBlueprintFromPath(
      "thruster", "assets/descriptionEntity/thruster.json");
  context._entityFactory.loadBlueprintFromPath(
      "explosion", "assets/descriptionEntity/explosion.json");
  context._entityFactory.loadBlueprintFromPath(
      "powerup_speed", "assets/powerUp/powerUp_speed.json");
  context._entityFactory.loadBlueprintFromPath(
      "enemyMecha", "assets/descriptionEntity/enemyMecha.json");
  context._entityFactory.loadBlueprintFromPath(
      "mecha_bullet", "assets/descriptionEntity/mecha_bullet.json");
  context._entityFactory.loadBlueprintFromPath(
      "bidos_death_effect", "assets/descriptionEntity/bidos_death_effect.json");

  context._entityFactory.loadBlueprintFromPath(
      "boss_room", "assets/descriptionEntity/boss_room.json");
  context._entityFactory.loadBlueprintFromPath(
      "enemySpinner", "assets/descriptionEntity/enemySpinner.json");
  context._entityFactory.loadBlueprintFromPath(
      "enemyPod", "assets/descriptionEntity/enemyPod.json");
  context._entityFactory.loadBlueprintFromPath(
      "enemyDart", "assets/descriptionEntity/enemyDart.json");
  context._entityFactory.loadBlueprintFromPath(
      "enemyArmoredPod", "assets/descriptionEntity/enemyArmoredPod.json");
  context._entityFactory.loadBlueprintFromPath(
      "enemyCargo", "assets/descriptionEntity/enemyCargo.json");
  context._entityFactory.loadBlueprintFromPath(
      "enemyGreenMecha", "assets/descriptionEntity/enemyGreenMecha.json");
  context._entityFactory.loadBlueprintFromPath(
      "green_mecha_bullet", "assets/descriptionEntity/green_mecha_bullet.json");
  context._entityFactory.loadBlueprintFromPath(
      "dobkeratops_body", "assets/descriptionEntity/dobkeratops_body.json");
  context._entityFactory.loadBlueprintFromPath(
      "dobkeratops_bullet", "assets/descriptionEntity/dobkeratops_bullet.json");
  context._entityFactory.loadBlueprintFromPath(
      "dobkeratops_alien", "assets/descriptionEntity/dobkeratops_alien.json");
  context._entityFactory.loadBlueprintFromPath(
      "dobkeratops_alien_bullet",
      "assets/descriptionEntity/dobkeratops_alien_bullet.json");
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
  context.registry.register_component<component::powerUp>();
  context.registry.register_component<component::DropPowerUp>();
  context.registry.register_component<component::DestroyAnimEnd>();
  context.registry.register_component<component::DeathEffect>();
}

void Game::createBindings() {
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
  _inputMapping.bindGamepad(2, rtk::ecs::concepts::GamepadButton::DpadLeft);
  _inputMapping.bindGamepadAxis(2, rtk::ecs::concepts::GamepadAxis::LeftX,
                                -0.30f);

  _inputMapping.bindKey(3, rtk::ecs::concepts::Key::Right);
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

  context.eventBus.subscribe<rtype::events::EnemyEscapedEvent>(
      [this, shootSoundId](const rtype::events::EnemyEscapedEvent &) {
        _adapterAudioManager.playSound(shootSoundId);
      });

  context.eventBus.subscribe<rtype::events::QuitGameEvent>(
      [this](const rtype::events::QuitGameEvent &) {
        _shouldQuit = true;
        _inputState.closeRequested = true;
      });
}

void Game::addSystems(rtk::EngineContext &context) {
  context.scheduler.add<rtk::systems::AnimationSystem>(rtk::ecs::Order::Render);
  context.scheduler.add<rtk::systems::PlayerSystem>(
      rtk::ecs::Order::Update, std::ref(_inputState),
      std::ref(context.eventBus), std::ref(context._entityFactory),
      std::ref(_networkClient));
  context.scheduler.add<rtk::systems::InputSystem<RtkWindowAdapter>>(
      rtk::ecs::Order::Input, std::ref(_adapter), _inputState, _inputMapping,
      false);
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
      _fontRtype);
}

bool Game::shouldContinue() const {
  return !_inputState.closeRequested && !_shouldQuit;
}

void Game::onUpdate(rtk::EngineContext &context, float dt) {
  updateNetworkStats();
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
        _gameStarting = true;
        _readyRequestPending = false;
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
      }

      auto view = context.registry.view<component::NetworkEntity>();
      for (auto e : view) {
        auto *netEnt =
            context.registry.get_components<component::NetworkEntity>().get(e);
        if (netEnt && netEnt->id == destroyPayload.networkId.get()) {
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

      for (const auto &netData : snapshotPayload.entities) {
        bool found = false;
        for (auto e : view) {
          auto *netEnt = networkEntities.get(e);
          if (netEnt && netEnt->id == netData.networkId.get()) {
            auto *transform = transforms.get(e);
            if (transform) {
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
            _networkClient.sendQueryEntity(netData.networkId.get());
          }
        }
      }
    }
  }

  if (_hudSystem) {
    _hudSystem->setInGameScene(_sceneManager.get<GameScene>() != nullptr);
  }

  const bool escapePressed =
      rtype::Input::hasAction(_inputState.actionMask,
                              rtype::Action::LeaveRoom) &&
      !rtype::Input::hasAction(_previousActionMask, rtype::Action::LeaveRoom);
  _previousActionMask = _inputState.actionMask;
  if (escapePressed && _sceneManager.get<RoomLobbyScene>() == nullptr)
    _shouldQuit = true;

  _sceneManager.update(context, _inputState, dt);
}

void Game::clearNetworkEntities(rtk::EngineContext &context) {
  std::vector<std::size_t> entities;
  for (auto entity : context.registry.view<component::NetworkEntity>())
    entities.push_back(entity);

  for (auto entity : entities) {
    context.registry.remove_entity_from_all_pools(entity);
    context.registry.kill_entity(entity);
  }
  _networkEntities.clear();
  _pendingEntityQueries.clear();
}

void Game::onUnload(rtk::EngineContext &context) {
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

  context.registry.kill_entity(it->second);
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
