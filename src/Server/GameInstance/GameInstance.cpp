#include "GameInstance.hpp"
#include "EngineContext.hpp"
#include "Scheduler.hpp"
#include "Server.hpp"
#include "components/AABBCollider.hpp"
#include "components/Dead.hpp"
#include "components/NetworkEntity.hpp"
#include "components/Transform.hpp"
#include "components/Velocity.hpp"
#include "components/Bullet.hpp"
#include "components/Controllable.hpp"
#include "components/DestroyableTile.hpp"
#include "components/Enemy.hpp"
#include "components/EnemyEvent.hpp"
#include "components/Health.hpp"
#include "ServerDeathSystem.hpp"
#include "systems/BulletSystem.hpp"
#include "systems/ColliderSystem.hpp"
#include "systems/MovementSystem.hpp"
#include "systems/EnemySpawnerSystem.hpp"
#include "systems/EnemyShootSystem.hpp"
#include "systems/EnemyGarbageCollectorSystem.hpp"
#include "NetworkIdGenerator.hpp"
#include "Protocol/EntityPayload.hpp"
#include "components/Velocity.hpp"
#include "components/Transform.hpp"
#include "components/Controllable.hpp"
#include "components/NetworkEntity.hpp"
#include "components/Transform.hpp"
#include "components/WaveMovement.hpp"
#include "systems/WaveMovementSystem.hpp"

#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>


namespace rtype::server {
  GameInstance::GameInstance(std::uint32_t roomId, Server &server)
    : _roomId(roomId),
    _dependency{*this, server},
    _sceneManager(_dependency, std::in_place_type<InstanceLobbyScene>)
  {}

  std::uint32_t GameInstance::getRoomId() const {
    return _roomId;
  }

  std::uint32_t GameInstance::getServerTick() const {
    return _serverTick;
  }

  void GameInstance::addPlayer(std::uint32_t connectionId, std::size_t entity) {
    _players.insert_or_assign(connectionId, entity);
  }

  bool GameInstance::removePlayer(std::uint32_t connectionId) {
    return _players.erase(connectionId) > 0;
  }

  void GameInstance::incrementServerTick() {
    _serverTick++;
  }

  bool GameInstance::hasPlayer(std::uint32_t connectionId) const {
    return _players.find(connectionId) != _players.end();
  }

  std::size_t GameInstance::getPlayerCount() const {
    return _players.size();
  }

  const std::unordered_map<std::uint32_t, std::size_t> &GameInstance::getPlayers() const {
    return _players;
  }

  bool GameInstance::isActive() const {
    return _isActive;
  }

  void GameInstance::setGameActive(bool active) {
    _isActive = active;
  }

  void GameInstance::resetGameState() {
    _isActive = false;
    _serverTick = 0;
  }

  rtk::EngineContext &GameInstance::getContext() {
    return _context;
  }

  void GameInstance::start(Server &server) {
    if (_systemsStarted)
      return;
   
    initialize();
    subscribeEvents(server);
    addSystems(server);
    _scheduler.start(_registry);
    _sceneManager.start(_context);
    _systemsStarted = true;
  }
  
  void GameInstance::update(float dt) {
    if (!_systemsStarted)
      return;

    _sceneManager.update(_context, _sceneInput, dt);
    _scheduler.run(_registry, dt);
    _registry.flush();
  }
  
  void GameInstance::stop() {
    if (!_systemsStarted)
      return;

    _sceneManager.stop(_context);
    _scheduler.stop(_registry);
    _systemsStarted = false;
  }

  void GameInstance::registerComponents(rtk::EngineContext &context) {
    context.registry.register_component<component::Transform>();
    context.registry.register_component<component::Velocity>();
    context.registry.register_component<component::AABBCollider>();
    context.registry.register_component<component::Dead>();
    context.registry.register_component<component::NetworkEntity>();
    context.registry.register_component<component::Controllable>();
    context.registry.register_component<component::Bullet>();
    context.registry.register_component<component::Enemy>();
    context.registry.register_component<rtype::events::EnemyEscapedEvent>();
    context.registry.register_component<component::Health>();
    context.registry.register_component<component::DestroyableTile>();
    context.registry.register_component<component::WaveMovement>();
  }
  
  void GameInstance::registerBuilders(rtk::EngineContext &context) {
    context._entityFactory.registerBuilder(
        "transform", component::transformFromJson);

    context._entityFactory.registerBuilder(
        "Transform", component::transformFromJson);

    context._entityFactory.registerBuilder(
        "velocity", component::velocityFromJson);

    context._entityFactory.registerBuilder(
        "Velocity", component::velocityFromJson);

    context._entityFactory.registerBuilder(
        "aabbCollider", component::aabbColliderFromJson);

    context._entityFactory.registerBuilder(
        "AABBCollider", component::aabbColliderFromJson);

    context._entityFactory.registerBuilder(
        "collider", component::aabbColliderFromJson);

    context._entityFactory.registerBuilder(
        "Collider", component::aabbColliderFromJson);

    context._entityFactory.registerBuilder(
        "dead", component::deadFromJson);

    context._entityFactory.registerBuilder(
        "Dead", component::deadFromJson);

    context._entityFactory.registerBuilder(
        "networkEntity", component::networkEntityFromJson);

    context._entityFactory.registerBuilder(
        "NetworkEntity", component::networkEntityFromJson);

    context._entityFactory.registerBuilder(
        "controllable", component::controllableFromJson);

    context._entityFactory.registerBuilder(
        "Controllable", component::controllableFromJson);

    context._entityFactory.registerBuilder(
        "bullet", component::bulletFromJson);

    context._entityFactory.registerBuilder(
        "Bullet", component::bulletFromJson);

    context._entityFactory.registerBuilder(
        "enemy", component::enemyFromJson);

    context._entityFactory.registerBuilder(
        "Enemy", component::enemyFromJson);

    context._entityFactory.registerBuilder(
        "enemyEvent", component::enemyEventFromJson);

    context._entityFactory.registerBuilder(
        "EnemyEvent", component::enemyEventFromJson);

    context._entityFactory.registerBuilder(
        "destroyableTile", component::destroyableTileFromJson);

    context._entityFactory.registerBuilder(
        "DestroyableTile", component::destroyableTileFromJson);

    context._entityFactory.registerBuilder(
        "health", component::healthFromJson);

    context._entityFactory.registerBuilder(
        "Health", component::healthFromJson);

    context._entityFactory.registerBuilder(
        "enemyEscapedEvent",
        rtype::events::enemyEscapedEventFromJson);

    context._entityFactory.registerBuilder(
        "EnemyEscapedEvent",
        rtype::events::enemyEscapedEventFromJson);
    context._entityFactory.registerBuilder(
        "waveMovement",
        component::waveMovementFromJson);
    context._entityFactory.registerBuilder(
        "WaveMovement",
        component::waveMovementFromJson);
  }
  
  void GameInstance::loadEntityBlueprints(rtk::EngineContext &context) {
    _playerArchetypes[0] =
      context._entityFactory.loadBlueprintFromPath(
        "player_blue",
        "assets/descriptionEntity/player_blue.json");
    _playerArchetypes[1] =
        context._entityFactory.loadBlueprintFromPath(
            "player_green",
            "assets/descriptionEntity/player_green.json");

    _playerArchetypes[2] =
        context._entityFactory.loadBlueprintFromPath(
            "player_pink",
            "assets/descriptionEntity/player_pink.json");

    _playerArchetypes[3] =
        context._entityFactory.loadBlueprintFromPath(
            "player_red",
            "assets/descriptionEntity/player_red.json");

    context._entityFactory.loadBlueprintFromPath(
        "player",
        "assets/descriptionEntity/player.json");

    context._entityFactory.loadBlueprintFromPath(
        "bullet",
        "assets/descriptionEntity/bullet.json");

    context._entityFactory.loadBlueprintFromPath(
        "enemyBidos",
        "assets/descriptionEntity/enemyBidos.json");

    context._entityFactory.loadBlueprintFromPath(
      "enemyRedShip",
      "assets/descriptionEntity/enemyRedShip.json");

    context._entityFactory.loadBlueprintFromPath(
        "gate",
        "assets/descriptionEntity/gate.json");

    context._entityFactory.loadBlueprintFromPath(
        "ramp",
        "assets/descriptionEntity/ramp.json");

    context._entityFactory.loadBlueprintFromPath(
        "reactor",
        "assets/descriptionEntity/reactor.json");

    context._entityFactory.loadBlueprintFromPath(
        "high_ground",
        "assets/descriptionEntity/high_ground.json");

    context._entityFactory.loadBlueprintFromPath(
        "low_ground",
        "assets/descriptionEntity/low_ground.json");

    context._entityFactory.loadBlueprintFromPath(
        "medium_ground",
        "assets/descriptionEntity/medium_ground.json");

    context._entityFactory.loadBlueprintFromPath(
        "enemy_bullet",
        "assets/descriptionEntity/enemy_bullet.json");
  }

  const std::array<std::uint16_t, 4> &GameInstance::getPlayerArchetypes() const {
    return _playerArchetypes;
  }


  void GameInstance::initialize() {
    if (_initialized)
      return;

    registerComponents(_context);
    registerBuilders(_context);
    loadEntityBlueprints(_context);
    _initialized = true;
  }

  void GameInstance::addSystems(Server& server) {
  if (_systemsRegistered)
      return;

  auto& context = _context;

  context.scheduler.add<ServerDeathSystem>(
      rtk::ecs::Order::Update,
      std::ref(server),
      std::ref(_players),
      std::cref(_serverTick)
  );

  context.scheduler.add<rtk::systems::BulletSystem>(
      rtk::ecs::Order::Update
  );

  context.scheduler.add<rtk::systems::EnemySpawnerSystem>(
      rtk::ecs::Order::Update,
      std::ref(context._entityFactory),
      std::ref(context.eventBus),
      [this]() {
          return _isActive;
      }
  );

  context.scheduler.add<rtk::systems::EnemyGCSystem>(
      rtk::ecs::Order::SecondUpdate,
      context.eventBus
  );

  context.scheduler.add<ServerDeathSystem>(
      rtk::ecs::Order::SecondUpdate,
      std::ref(server),
      std::ref(_players),
      std::cref(_serverTick)
  );

  context.scheduler.add<rtk::systems::MovementSystem>(
      rtk::ecs::Order::Physics
  );

  context.scheduler.add<rtk::systems::ColliderSystem>(
      rtk::ecs::Order::Physics,
      _collisionBuffer,
      64.f,
      context.eventBus
  );

  context.scheduler.add<rtk::systems::EnemyShootSystem>(
      rtk::ecs::Order::Update,
      std::ref(context._entityFactory),
      std::ref(server),
      std::ref(_players)
  );

  context.scheduler.add<rtk::systems::WaveMovementSystem>(
      rtk::ecs::Order::Update);

  _systemsRegistered = true;
}

  void GameInstance::subscribeEvents(Server& server) {
    if (_eventsRegistered)
      return;

  _context.eventBus.subscribe<rtype::events::EnemySpawnedEvent>(
    [this, &server](const rtype::events::EnemySpawnedEvent& event) {
    const auto payload = protocol::createEntityPayload(
      _serverTick, event.networkId,
      _context._entityFactory.getArchetypeID("enemyBidos"),
      event.x, event.y);

    for (const auto& [connectionId, entity] : _players)
      server.sendCreateEntity(connectionId, payload);
    });
  
  _context.eventBus.subscribe<rtk::collision::Collision>(
    [this](const rtk::collision::Collision& event) {

        auto& colliders =
            _context.registry.get_components<component::AABBCollider>();

        auto* colA = colliders.get(event.firstEntity);
        auto* colB = colliders.get(event.secondEntity);

        if (!colA || !colB)
            return;

        const std::uint32_t collisionPair =
            colA->layer | colB->layer;

        const std::uint32_t bulletVsEnemy =
            Collider::CollisionLayer::PlayerProjectile |
            Collider::CollisionLayer::Enemy;

        const std::uint32_t bulletVsEnv =
            Collider::CollisionLayer::PlayerProjectile |
            Collider::CollisionLayer::Environment;

        const std::uint32_t playerVsEnemy =
            Collider::CollisionLayer::Player |
            Collider::CollisionLayer::Enemy;

        const std::uint32_t playerVsEnv =
            Collider::CollisionLayer::Player |
            Collider::CollisionLayer::Environment;

        const std::uint32_t playerVsEnemyProj =
            Collider::CollisionLayer::Player |
            Collider::CollisionLayer::EnemyProjectile;

        auto& deads =
            _context.registry.get_components<component::Dead>();

        auto& healths =
            _context.registry.get_components<component::Health>();

        auto& bullets =
            _context.registry.get_components<component::Bullet>();

        if (collisionPair == bulletVsEnemy) {

            deads.insert_at(event.firstEntity, {});
            deads.insert_at(event.secondEntity, {});

        } else if (collisionPair == bulletVsEnv) {

            const bool firstIsBullet =
                colA->layer ==
                Collider::CollisionLayer::PlayerProjectile;

            const std::size_t bulletEnt =
                firstIsBullet
                    ? event.firstEntity
                    : event.secondEntity;

            const std::size_t tileEnt =
                colA->layer == Collider::CollisionLayer::Environment
                    ? event.firstEntity
                    : event.secondEntity;

            deads.insert_at(bulletEnt, {});

            auto* bulletComp = bullets.get(bulletEnt);
            auto* healthComp = healths.get(tileEnt);

            if (healthComp && bulletComp) {

                healthComp->current -= bulletComp->damage;

                if (healthComp->current <= 0.0f) {
                    deads.insert_at(tileEnt, {});

                    std::cout
                        << "[SERVER] Tile destroyed!"
                        << std::endl;
                }
            }

        } else if (collisionPair == playerVsEnemy) {

            deads.insert_at(event.firstEntity, {});
            deads.insert_at(event.secondEntity, {});

            std::cout
                << "[SERVER] Player died from collision!"
                << std::endl;

        } else if (collisionPair == playerVsEnv) {

            if (colA->layer == Collider::CollisionLayer::Player) {
                deads.insert_at(event.firstEntity, {});
            } else {
                deads.insert_at(event.secondEntity, {});
            }

        } else if (collisionPair == playerVsEnemyProj) {

            deads.insert_at(event.firstEntity, {});
            deads.insert_at(event.secondEntity, {});

            std::cout
                << "[SERVER] Player hit by enemy bullet!"
                << std::endl;
        }
    });
   _eventsRegistered = true;
}

void GameInstance::processInput(const Server::InputEvent& inputEvent, Server& server) {
  auto it = _players.find(inputEvent.connectionId);
  if (it == _players.end())
    return;

  const std::size_t pEntity = it->second;
  if (pEntity == NO_ENTITY)
    return;
  auto &registry = _context.registry;
  auto *velocity = registry.get_components<component::Velocity>().get(pEntity);
  auto* controllable = registry.get_components<component::Controllable>().get(pEntity);
  auto* transform = registry.get_components<component::Transform>().get(pEntity);
  if (!velocity || !controllable || !transform)
    return;

  velocity->x = 0.0f;
  velocity->y = 0.0f;
  const auto actions = inputEvent.input.actions;
    if (rtype::protocol::hasAction(actions, rtype::protocol::InputAction::Up)) {
      velocity->y = -controllable->speed;
    }
    if (rtype::protocol::hasAction(actions,
                                   rtype::protocol::InputAction::Down)) {
      velocity->y = controllable->speed;
    }
    if (rtype::protocol::hasAction(actions,
                                   rtype::protocol::InputAction::Left)) {
      velocity->x = -controllable->speed;
    }
    if (rtype::protocol::hasAction(actions,
                                   rtype::protocol::InputAction::Right)) {
      velocity->x = controllable->speed;
    }
    if (velocity->x != 0.0f && velocity->y != 0.0f) {
      velocity->x *= 0.7071f;
      velocity->y *= 0.7071f;
    }
    const bool isShoot = rtype::protocol::hasAction(
        actions, rtype::protocol::InputAction::Shoot);
    if (isShoot) {
      controllable->isChargingShoot = true;
    } else if (controllable->isChargingShoot) {
      controllable->isChargingShoot = false;

      std::uint32_t bulletNetId =
          rtype::server::NetworkIdGenerator::getInstance().getNextId();

      auto bulletEntity = _context._entityFactory.createNetwork(
          _context.registry, "bullet", bulletNetId);
      auto *bulletTransform =
          _context.registry.get_components<component::Transform>().get(
              bulletEntity);

      if (bulletTransform) {
        bulletTransform->position = {transform->position.x + 50.0f,
                                     transform->position.y};
        const auto payload = protocol::createEntityPayload(
          _serverTick,
          bulletNetId,
          _context._entityFactory.getArchetypeID("bullet"),
          bulletTransform->position.x,
          bulletTransform->position.y
        );


        for (const auto &[connectedClientId, entity] : _players) {
          server.sendCreateEntity(connectedClientId, payload);
        }
      }
    }
}

bool GameInstance::addConnectedPlayer(std::uint32_t connectionId, Server& server) {
  if (hasPlayer(connectionId))
    return false;

  if (_players.size() >= 4)
    return false;

  initialize();
  auto &registry = _context.registry;
  std::array<bool, 4> slots = {false, false, false, false};
  for (const auto &[id, entity] : _players) {
    (void)id;

    auto *transform = registry.get_components<component::Transform>().get(entity);
    if (!transform)
      continue;
    
    int slot = static_cast<int>(std::lround((transform->position.y -150.f) / 100.f));
    if (slot >= 0 && slot < 4)
      slots[slot] = true;
  }
  std::uint32_t slot = 0;

  while (slot < 4 && slots[slot])
    ++slot;  
  if (slot == 4)
    return false;
 
  sendExistingEntities(connectionId, server);
  const std::uint32_t networkId = NetworkIdGenerator::getInstance().getNextId();
  const auto archetypeId = _playerArchetypes[slot];
  const auto entity = _context._entityFactory.createNetwork(
    registry,
    archetypeId,
    networkId
  );

  auto *transform = registry.get_components<component::Transform>().get(entity);
  if (!transform)
    return false;
  transform->position = {
    150.0f,
    150.0f + static_cast<float>(slot) * 100.0f
  };
  _players.emplace(connectionId, entity);
  const auto payload = protocol::createEntityPayload(
    _serverTick,
    networkId,
    archetypeId,
    transform->position.x,
    transform->position.y,
    transform->rotation
  );
  for (const auto& [clientId, playerEntity] : _players) {
    (void)playerEntity;
    server.sendCreateEntity(clientId, payload);
  }

  std::cout
    << "[GameInstance " << _roomId << "] Player "
    << connectionId
    << " created (NetworkId: " << networkId
    << ", Slot: " << slot << ")"
    << std::endl;
  return true;
}

void GameInstance::sendExistingEntities(std::uint32_t connectionId, Server& server) {
  auto view =
      _context.registry.view<component::NetworkEntity, component::Transform>();
  auto &transforms = _context.registry.get_components<component::Transform>();
  auto &networkEntities =
      _context.registry.get_components<component::NetworkEntity>();

  for (auto entity : view) {
    const auto *transform = transforms.get(entity);
    const auto *netEnt = networkEntities.get(entity);
    if (!transform || !netEnt)
      continue;

    if (!std::isfinite(transform->position.x) ||
        !std::isfinite(transform->position.y) ||
        transform->position.x < protocol::MIN_POSITION ||
        transform->position.x > protocol::MAX_POSITION ||
        transform->position.y < protocol::MIN_POSITION ||
        transform->position.y > protocol::MAX_POSITION) {
      continue;
    }

    const auto payload = protocol::createEntityPayload(
        _serverTick, netEnt->id, netEnt->archetypeId, transform->position.x,
        transform->position.y, transform->rotation);
    server.sendCreateEntity(connectionId, payload);
  }
}

bool GameInstance::removeConnectedPlayer( std::uint32_t connectionId, Server& server) {
  auto it = _players.find(connectionId);
  if (it == _players.end())
    return false;

  const auto entity = it->second;
  if (entity == NO_ENTITY) {
    _players.erase(it);
    return true;
  }
    
  auto &registry = _context.registry;
  auto *netEnt = registry.get_components<component::NetworkEntity>().get(entity);
  
  if (netEnt) {
    const auto payload = protocol::destroyEntityPayload(_serverTick, netEnt->id, protocol::DestroyReason::Unknown);
    for (const auto &[clientId, Pentity]: _players) {
      (void)Pentity;
      if (clientId == connectionId)
        continue;
      server.sendDestroyEntity(clientId, payload);
    }
  }
  
  registry.remove_entity_from_all_pools(entity);
  registry.kill_entity(entity);
  _players.erase(it);
  return true;
}

void GameInstance::sendSnapshots(Server& server) {
  auto& registry = _context.registry;
  auto view = registry.view<component::NetworkEntity, component::Transform>();
  auto& transforms = registry.get_components<component::Transform>();
  auto& networkEntities = registry.get_components<component::NetworkEntity>();
  protocol::SnapshotPayload snapshot;
  snapshot.serverTick = _serverTick;

  const auto flush = [&]() {
    if (snapshot.entities.empty())
      return;

    for (const auto& [connectionId, entity] : _players) {
      (void)entity;
      server.sendSnapshot(connectionId, snapshot);
    }
    snapshot.entities.clear();
  };

  for (auto entity : view) {
    const auto* transform = transforms.get(entity);
    const auto* netEnt = networkEntities.get(entity);

    if (!transform || !netEnt)
      continue;

    if (!std::isfinite(transform->position.x) ||
      !std::isfinite(transform->position.y) ||
      transform->position.x < protocol::MIN_POSITION ||
      transform->position.x > protocol::MAX_POSITION ||
      transform->position.y < protocol::MIN_POSITION ||
      transform->position.y > protocol::MAX_POSITION)
      continue;

    snapshot.entities.push_back({
      netEnt->id,
      static_cast<std::uint8_t>(
          netEnt->archetypeId & 0xFF
      ),
      protocol::encodePosition(transform->position.x),
      protocol::encodePosition(transform->position.y),
      protocol::encodeRotation(transform->rotation)
    });

    if (snapshot.entities.size() ==
      protocol::SNAPSHOT_MAX_ENTITIES) {
      flush();
    }
  }

  flush();
}
void GameInstance::processEntityQuery(const Server::QueryEntityEvent& queryEvent, Server& server) {
    if (!hasPlayer(queryEvent.connectionId))
        return;

    auto& registry = _context.registry;
    auto view = registry.view<
        component::NetworkEntity,
        component::Transform
    >();

    auto& transforms = registry.get_components<component::Transform>();
    auto& networkEntities = registry.get_components<component::NetworkEntity>();

    for (auto entity : view) {
        const auto* netEnt = networkEntities.get(entity);
        if (!netEnt || netEnt->id != queryEvent.networkId)
            continue;

        const auto* transform = transforms.get(entity);
        if (!transform)
            break;

        if (!std::isfinite(transform->position.x) || !std::isfinite(transform->position.y) ||
            transform->position.x < protocol::MIN_POSITION ||
            transform->position.x > protocol::MAX_POSITION ||
            transform->position.y < protocol::MIN_POSITION ||
            transform->position.y > protocol::MAX_POSITION) {
            break;
        }

        const auto payload = protocol::createEntityPayload(
            _serverTick, netEnt->id,
            netEnt->archetypeId, transform->position.x,
            transform->position.y, transform->rotation);

        server.sendCreateEntity(queryEvent.connectionId, payload);
        break;
    }
}


float GameInstance::createGate(float startX, bool symmetric) {
  auto gateEntity = _context._entityFactory.createNetwork(
      _context.registry, "gate",
      rtype::server::NetworkIdGenerator::getInstance().getNextId());
  auto *transform =
      _context.registry.get_components<component::Transform>().get(gateEntity);
  if (transform) {
    transform->position.x = startX;
  }

  if (symmetric) {
    auto ceilingEntity = _context._entityFactory.createNetwork(
        _context.registry, "gate",
        rtype::server::NetworkIdGenerator::getInstance().getNextId());
    auto *ceilTransform =
        _context.registry.get_components<component::Transform>().get(
            ceilingEntity);
    if (ceilTransform) {
      ceilTransform->position.x = startX;
      ceilTransform->position.y = 0.0f;
    }
  }
  return startX + (192.0f * 2.0f);
}

float GameInstance::createRamp(float startX, bool symmetric) {
  auto rampEntity = _context._entityFactory.createNetwork(
      _context.registry, "ramp",
      rtype::server::NetworkIdGenerator::getInstance().getNextId());
  auto *transform =
      _context.registry.get_components<component::Transform>().get(rampEntity);
  if (transform) {
    transform->position.x = startX;
  }

  if (symmetric) {
    auto ceilingEntity = _context._entityFactory.createNetwork(
        _context.registry, "ramp",
        rtype::server::NetworkIdGenerator::getInstance().getNextId());
    auto *ceilTransform =
        _context.registry.get_components<component::Transform>().get(
            ceilingEntity);
    if (ceilTransform) {
      ceilTransform->position.x = startX;
      ceilTransform->position.y = 0.0f;
    }
  }
  return startX + (192.0f * 2.0f);
}

float GameInstance::createReactor(float startX, bool symmetric) {
  auto reactorEntity = _context._entityFactory.createNetwork(
      _context.registry, "reactor",
      rtype::server::NetworkIdGenerator::getInstance().getNextId());
  auto *transform = _context.registry.get_components<component::Transform>().get(
      reactorEntity);
  if (transform) {
    transform->position.x = startX;
  }

  if (symmetric) {
    auto ceilingEntity = _context._entityFactory.createNetwork(
        _context.registry, "reactor",
        rtype::server::NetworkIdGenerator::getInstance().getNextId());
    auto *ceilTransform =
        _context.registry.get_components<component::Transform>().get(
            ceilingEntity);
    if (ceilTransform) {
      ceilTransform->position.x = startX;
      ceilTransform->position.y = 0.0f;
    }
  }
  return startX + (113.0f * 2.0f);
}

float GameInstance::createHighGround(float startX, bool symmetric) {
  auto highGroundEntity = _context._entityFactory.createNetwork(
      _context.registry, "high_ground",
      rtype::server::NetworkIdGenerator::getInstance().getNextId());
  auto *transform = _context.registry.get_components<component::Transform>().get(
      highGroundEntity);
  if (transform) {
    transform->position.x = startX;
  }

  if (symmetric) {
    auto ceilingEntity = _context._entityFactory.createNetwork(
        _context.registry, "high_ground",
        rtype::server::NetworkIdGenerator::getInstance().getNextId());
    auto *ceilTransform =
        _context.registry.get_components<component::Transform>().get(
            ceilingEntity);
    if (ceilTransform) {
      ceilTransform->position.x = startX;
      ceilTransform->position.y = 0.0f;
    }
  }
  return startX + (64.0f * 2.0f);
}

float GameInstance::createLowGround(float startX, bool symmetric) {
  auto lowGroundEntity = _context._entityFactory.createNetwork(
      _context.registry, "low_ground",
      rtype::server::NetworkIdGenerator::getInstance().getNextId());
  auto *transform = _context.registry.get_components<component::Transform>().get(
      lowGroundEntity);
  if (transform) {
    transform->position.x = startX;
  }

  if (symmetric) {
    auto ceilingEntity = _context._entityFactory.createNetwork(
        _context.registry, "low_ground",
        rtype::server::NetworkIdGenerator::getInstance().getNextId());
    auto *ceilTransform =
        _context.registry.get_components<component::Transform>().get(
            ceilingEntity);
    if (ceilTransform) {
      ceilTransform->position.x = startX;
      ceilTransform->position.y = 0.0f;
    }
  }
  return startX + (256.0f * 2.0f);
}

float GameInstance::createMediumGround(float startX, bool symmetric) {
  auto groundEntity = _context._entityFactory.createNetwork(
      _context.registry, "medium_ground",
      rtype::server::NetworkIdGenerator::getInstance().getNextId());
  auto *transform =
      _context.registry.get_components<component::Transform>().get(groundEntity);
  if (transform) {
    transform->position.x = startX;
  }

  if (symmetric) {
    auto ceilingEntity = _context._entityFactory.createNetwork(
        _context.registry, "medium_ground",
        rtype::server::NetworkIdGenerator::getInstance().getNextId());
    auto *ceilTransform =
        _context.registry.get_components<component::Transform>().get(
            ceilingEntity);
    if (ceilTransform) {
      ceilTransform->position.x = startX;
      ceilTransform->position.y = 0.0f;
    }
  }
  return startX + (64.0f * 2.0f);
}

void GameInstance::createBackground()
{
  _currentStageId.clear();
  _nextStageId.clear();
  _stageName.clear();

  _stageTimer = 0.0f;
  _stageDuration = 0.0f;
  _stageObstaclesSpawned = 0;

  _stageActive = false;
  _allStagesCompleted = false;

  loadStageManifest(_context);
}

void GameInstance::endGame(Server &server) {
  auto &registry = _context.registry;
  auto &netEntities = registry.get_components<component::NetworkEntity>();
  for (auto entity : registry.view<component::NetworkEntity>()) {
    const auto *netEnt = netEntities.get(entity);
    if (!netEnt || netEnt->id == 0)
      continue;

    const auto payload = protocol::destroyEntityPayload(_serverTick,
        netEnt->id, protocol::DestroyReason::GameEnded);
    
    for (const auto &[connectionId, playerEnt] : _players) {
      (void) playerEnt;
      server.sendDestroyEntity(connectionId, payload);
    }
  }
  std::vector<std::size_t> entities;

  for (auto entity : registry.view<component::Transform>())
    entities.push_back(entity);
  
  for (auto entity : entities) {
    registry.remove_entity_from_all_pools(entity);
    registry.kill_entity(entity);
  }
  registry.flush();

  for (auto &[connectionId, entity] : _players) {
    (void) connectionId;
    entity = NO_ENTITY;
  }
  _isActive = false;
  _serverTick = 0;
}

void GameInstance::respawnPlayers() {
  auto &registry = _context.registry;
  std::vector<std::uint32_t> connectionIds;

  for (auto &[connectionId, entity] : _players) {
    (void) entity;
    connectionIds.push_back(connectionId);
  }

  std::sort(connectionIds.begin(), connectionIds.end());
  std::size_t slot = 0;

  for (const auto &connectionId : connectionIds) {
    if (slot >= _playerArchetypes.size())
      break;
    const auto netId = NetworkIdGenerator::getInstance().getNextId();
    const auto archId = _playerArchetypes[slot];
    const auto entity = _context._entityFactory.createNetwork(registry, archId, netId);
    
    auto *transform = registry.get_components<component::Transform>().get(entity);
    if (!transform) {
      registry.remove_entity_from_all_pools(entity);
      registry.kill_entity(entity);
    }

    transform->position = {150.0f, 150.0f + static_cast<float>(slot) * 100.0f};
    _players[connectionId] = entity;
    ++slot;
  }
}


bool GameInstance::hasAlivePlayers() {
  auto &transform = _registry.get_components<component::Transform>();

  for (const auto &[connectionId, entity] : _players) {
    (void)connectionId;

    if (entity == NO_ENTITY)
      continue;
    if (transform.get(entity) != nullptr)
      return true;
  }
  return false;

}

float GameInstance::spawnStageSegment(
  rtk::EngineContext &context, const std::string &blueprint,
  float startX, float spacing, bool symmetric) {
  auto groundEntity = context._entityFactory.createNetwork(
    context.registry,
    blueprint,
    NetworkIdGenerator::getInstance().getNextId());

  auto *transform =context.registry.get_components<component::Transform>().get(groundEntity);

  if (transform) 
    transform->position.x = startX;

  if (symmetric) {
    auto ceilingEntity = context._entityFactory.createNetwork(
      context.registry,
      blueprint,
      NetworkIdGenerator::getInstance().getNextId());
    auto *ceilTransform = context.registry.get_components<component::Transform>().get(ceilingEntity);
    if (ceilTransform) {
        ceilTransform->position.x = startX;
        ceilTransform->position.y = 0.0f;
    }
  }
  return startX + spacing;
}

void GameInstance::loadStage(rtk::EngineContext &context, const std::string &stagePath) {
  std::ifstream file(stagePath);
  if (!file.is_open()) {
    std::cerr << "[Server] Failed to open stage file: " << stagePath << std::endl;
    return;
  }
  nlohmann::json stageJson;
  try {
    file >> stageJson;
  }
  catch (const std::exception &e) {
    std::cerr << "[Server] Failed to parse stage JSON (" << stagePath << "): "<< e.what() << std::endl;
    return;
  }

  _stageName = stageJson.value("name", "Unnamed Stage");
  _nextStageId = stageJson.value("nextStage", "");
  _stageDuration = stageJson.value("duration", 0.0f);
  _stageTimer = 0.0f;
  _stageObstaclesSpawned = 0;
  _stageActive = true;

  float currentX = stageJson.value("startX", 0.0f);
  if (!stageJson.contains("segments") ||
    !stageJson["segments"].is_array()) {
    std::cerr << "[Server] Stage file missing 'segments' array: " << stagePath << std::endl;
    return;
  }

  for (const auto &segment : stageJson["segments"]) {
    const std::string blueprint = segment.value("blueprint", "");

    const bool symmetric = segment.value("symmetric", false);

    const float spacing = segment.value("spacing", 0.0f);

    if (blueprint.empty())
      continue;

    currentX = spawnStageSegment(context, blueprint, currentX, spacing, symmetric);
    _stageObstaclesSpawned += symmetric ? 2 : 1;
  }

  std::cout << "[Server] Loaded stage from " << stagePath << " (" << _stageName << ") with "
    << _stageObstaclesSpawned << " obstacle entities" << std::endl;
}

bool GameInstance::loadStageById(rtk::EngineContext &context, 
    const std::string &stageId, const std::string &manifestPath) {
  std::ifstream file(manifestPath);
  if (!file.is_open()) {
    std::cerr << "[Server] Failed to open stage manifest: " << manifestPath << std::endl;
    return false;
  }
  nlohmann::json manifestJson;
  try {
      file >> manifestJson;
  } catch (const std::exception &e) {
    std::cerr << "[Server] Failed to parse stage manifest ("
      << manifestPath << "): " << e.what() << std::endl;
    return false;
  }

  std::string stageFilePath;
  std::string manifestNextStage;
  if (manifestJson.contains("stages") &&
    manifestJson["stages"].is_array()) {

    for (const auto &stageEntry : manifestJson["stages"]) {
      if (stageEntry.value("id", "") == stageId) {
        stageFilePath = stageEntry.value("file", "");
        manifestNextStage = stageEntry.value("nextStage", "");
        break;
      }
    }
  }

  if (stageFilePath.empty()) {
    std::cerr << "[Server] Stage id not found in manifest: " << stageId << std::endl;
    return false;
  }
  loadStage(context, stageFilePath);
  _currentStageId = stageId;
  if (_nextStageId.empty() && !manifestNextStage.empty())
    _nextStageId = manifestNextStage;
  return true;
}

void GameInstance::loadStageManifest(rtk::EngineContext &context, const std::string &manifestPath) {
  std::ifstream file(manifestPath);
  if (!file.is_open()) {
    std::cerr << "[Server] Failed to open stage manifest: " << manifestPath << std::endl;
    return;
  }
  nlohmann::json manifestJson;
  try {
    file >> manifestJson;
  }
  catch (const std::exception &e) {
    std::cerr << "[Server] Failed to parse stage manifest (" <<
      manifestPath << "): " << e.what() << std::endl;
    return;
  }

  const std::string defaultStageId =
    manifestJson.value("defaultStage", "stage1");

  loadStageById(context, defaultStageId, manifestPath);
}

bool GameInstance::hasActiveEnvironmentEntities(rtk::EngineContext &context) const {
  auto view = context.registry.view<component::Transform, component::AABBCollider>();
  auto &colliders = context.registry.get_components<component::AABBCollider>();
  auto &deads = context.registry.get_components<component::Dead>();

  for (auto entity : view) {
    if (deads.get(entity) != nullptr)
      continue;
    const auto *col = colliders.get(entity);
    if (col && (col->layer & Collider::CollisionLayer::Environment)) {
      return true;
    }
  }
  return false;
}

void GameInstance::clearEnvironmentEntities(rtk::EngineContext &context) {
  auto view = context.registry.view<component::Transform, component::AABBCollider>();
  auto &colliders = context.registry.get_components<component::AABBCollider>();
  auto &deads = context.registry.get_components<component::Dead>();

  for (auto entity : view) {
    const auto *col = colliders.get(entity);

    if (col && (col->layer & Collider::CollisionLayer::Environment)) {
      deads.insert_at(entity, {});
    }
  }
}

void GameInstance::clearEnemies(rtk::EngineContext &context) {
  auto view = context.registry.view<component::Enemy>();
  auto &deads = context.registry.get_components<component::Dead>();

  for (auto entity : view)
    deads.insert_at(entity, {});
}

bool GameInstance::isStageFinished(rtk::EngineContext &context) const {
  if (!_stageActive)
    return false;

  if (_stageDuration > 0.0f && _stageTimer >= _stageDuration)
    return true;

  if (_stageObstaclesSpawned > 0 && !hasActiveEnvironmentEntities(context))
    return true;
  return false;
}

bool GameInstance::advanceToNextStage(rtk::EngineContext &context) {
  if (_nextStageId.empty()) {
    std::cout << "[Server] No next stage configured after "
      << _currentStageId << std::endl;
      _stageActive = false;
      return false;
  }
  clearEnvironmentEntities(context);
  clearEnemies(context);
  const std::string nextId = _nextStageId;
  _nextStageId.clear();
  if (!loadStageById(context, nextId)) {
    std::cerr << "[Server] Failed to load next stage: " << nextId << std::endl;
    _stageActive = false;
    return false;
  }
  for (const auto &[playerId, entity] : _players) {
    (void)entity;
    sendExistingEntities(playerId, _dependency.server);
  }
  return true;
}

void GameInstance::updateStage(float dt)
{
  auto &context = _context;
  if (!_isActive || !_stageActive)
    return;
  _stageTimer += dt;
  if (isStageFinished(context)) {
    if (!advanceToNextStage(context)) {
      _allStagesCompleted = true;
    }
  }
}


} //namespace rtype::server
