#include "game.hpp"

#include <iostream>

#include "NetworkIdGenerator.hpp"

#include "EngineContext.hpp"
#include "Protocol/EntityPayload.hpp"
#include "Protocol/SnapshotPayload.hpp"
#include "components/Transform.hpp"
#include "components/Velocity.hpp"
#include "components/AABBCollider.hpp"
#include "components/Dead.hpp"
#include "components/NetworkEntity.hpp"

#include "components/Controllable.hpp"
#include "components/Bullet.hpp"
#include "components/Enemy.hpp"
#include "components/EnemyEvent.hpp"

#include "systems/MovementSystem.hpp"
#include "systems/ColliderSystem.hpp"
#include "systems/DeadSystem.hpp"
#include "systems/BulletSystem.hpp"
#include "systems/EnemySpawnerSystem.hpp"
#include "systems/EnemyGarbageCollectorSystem.hpp"

namespace rtype::server {

Game::Game(std::uint16_t port) : _server(port) {}

Game::~Game() = default;

void Game::onLoad(rtk::EngineContext &context) {
  registerComponents(context);
  registerBuilders(context);
  loadEntityBlueprints(context);
  subscribeEvents(context);
  addSystems(context);

  createBackground(context);

  _server.start();
  std::cout << "[Server] Game simulation started" << std::endl;
}

void Game::registerComponents(rtk::EngineContext &context) {
  context.registry.register_component<component::Transform>();
  context.registry.register_component<component::Velocity>();
  context.registry.register_component<component::AABBCollider>();
  context.registry.register_component<component::Dead>();
  context.registry.register_component<component::NetworkEntity>();
  context.registry.register_component<component::Controllable>();
  context.registry.register_component<component::Bullet>();
  context.registry.register_component<component::Enemy>();
  context.registry.register_component<rtype::events::EnemyEscapedEvent>();
}

void Game::registerBuilders(rtk::EngineContext &context) {
  context._entityFactory.registerBuilder("transform", component::transformFromJson);
  context._entityFactory.registerBuilder("Transform", component::transformFromJson);
  context._entityFactory.registerBuilder("velocity", component::velocityFromJson);
  context._entityFactory.registerBuilder("Velocity", component::velocityFromJson);
  context._entityFactory.registerBuilder("aabbCollider", component::aabbColliderFromJson);
  context._entityFactory.registerBuilder("AABBCollider", component::aabbColliderFromJson);
  context._entityFactory.registerBuilder("collider", component::aabbColliderFromJson);
  context._entityFactory.registerBuilder("Collider", component::aabbColliderFromJson);
  context._entityFactory.registerBuilder("dead", component::deadFromJson);
  context._entityFactory.registerBuilder("Dead", component::deadFromJson);
  context._entityFactory.registerBuilder("networkEntity", component::networkEntityFromJson);
  context._entityFactory.registerBuilder("NetworkEntity", component::networkEntityFromJson);
  context._entityFactory.registerBuilder("controllable", component::controllableFromJson);
  context._entityFactory.registerBuilder("Controllable", component::controllableFromJson);
  context._entityFactory.registerBuilder("bullet", component::bulletFromJson);
  context._entityFactory.registerBuilder("Bullet", component::bulletFromJson);
  context._entityFactory.registerBuilder("enemy", component::enemyFromJson);
  context._entityFactory.registerBuilder("Enemy", component::enemyFromJson);
  context._entityFactory.registerBuilder("enemyEvent", component::enemyEventFromJson);
  context._entityFactory.registerBuilder("EnemyEvent", component::enemyEventFromJson);
  context._entityFactory.registerBuilder("enemyEscapedEvent", rtype::events::enemyEscapedEventFromJson);
  context._entityFactory.registerBuilder("EnemyEscapedEvent", rtype::events::enemyEscapedEventFromJson);
}

void Game::loadEntityBlueprints(rtk::EngineContext &context) {
  context._entityFactory.loadBlueprintFromPath("player", "assets/descriptionEntity/player.json");
  context._entityFactory.loadBlueprintFromPath("bullet", "assets/descriptionEntity/bullet.json");
  context._entityFactory.loadBlueprintFromPath("enemyBidos", "assets/descriptionEntity/enemyBidos.json");
  context._entityFactory.loadBlueprintFromPath("gate", "assets/descriptionEntity/gate.json");
  context._entityFactory.loadBlueprintFromPath("ramp", "assets/descriptionEntity/ramp.json");
  context._entityFactory.loadBlueprintFromPath("reactor", "assets/descriptionEntity/reactor.json");
  context._entityFactory.loadBlueprintFromPath("high_ground", "assets/descriptionEntity/high_ground.json");
  context._entityFactory.loadBlueprintFromPath("low_ground", "assets/descriptionEntity/low_ground.json");
  context._entityFactory.loadBlueprintFromPath("medium_ground", "assets/descriptionEntity/medium_ground.json");
}

void Game::subscribeEvents(rtk::EngineContext &context) {

  context.eventBus.subscribe<rtype::events::EnemySpawnedEvent>(
      [this, &context](const rtype::events::EnemySpawnedEvent &event) {
          std::cout << "[Create]: BIDOS\n";
          protocol::CreateEntityPayload payload;
          payload.entityId = event.networkId;
          payload.blueprintId = context._entityFactory.getArchetypeID("enemyBidos");
          payload.x = event.x;
          payload.y = event.y;

          for (const auto &[playerId, entity] : _players) {
              _server.sendCreateEntity(playerId, payload);
          }
      }
  );

  context.eventBus.subscribe<rtk::collision::Collision>(
      [&context](const rtk::collision::Collision &event) {
        auto &colliders = context.registry.get_components<component::AABBCollider>();
        auto *colA = colliders.get(event.firstEntity);
        auto *colB = colliders.get(event.secondEntity);

        if (!colA || !colB) return;

        std::uint32_t collisionPair = colA->layer | colB->layer;
        std::uint32_t bulletVsEnemy =
            Collider::CollisionLayer::PlayerProjectile | Collider::CollisionLayer::Enemy;
        std::uint32_t bulletVsEnv =
            Collider::CollisionLayer::PlayerProjectile | Collider::CollisionLayer::Environment;
        std::uint32_t playerVsEnemy =
            Collider::CollisionLayer::Player | Collider::CollisionLayer::Enemy;
        std::uint32_t playerVsEnv =
            Collider::CollisionLayer::Player | Collider::CollisionLayer::Environment;

        auto &deads = context.registry.get_components<component::Dead>();

        if (collisionPair == bulletVsEnemy) {
          deads.insert_at(event.firstEntity, {});
          deads.insert_at(event.secondEntity, {});
        } else if (collisionPair == bulletVsEnv) {
          if (colA->layer == Collider::CollisionLayer::PlayerProjectile) {
            deads.insert_at(event.firstEntity, {});
          } else {
            deads.insert_at(event.secondEntity, {});
          }
        } else if (collisionPair == playerVsEnemy) {
          deads.insert_at(event.firstEntity, {});
          deads.insert_at(event.secondEntity, {});
          std::cout << "[SERVER] Player died from collision!" << std::endl;
        } else if (collisionPair == playerVsEnv) {
          if (colA->layer == Collider::CollisionLayer::Player) {
            deads.insert_at(event.firstEntity, {});
          } else {
            deads.insert_at(event.secondEntity, {});
          }
        }
      });
}

void Game::addSystems(rtk::EngineContext &context) {
  context.scheduler.add<rtk::systems::DeathSystem>(rtk::ecs::Order::Update);
  context.scheduler.add<rtk::systems::BulletSystem>(rtk::ecs::Order::Update);
  context.scheduler.add<rtk::systems::EnemySpawnerSystem>(
    rtk::ecs::Order::Update, std::ref(context._entityFactory), std::ref(context.eventBus));
  context.scheduler.add<rtk::systems::EnemyGCSystem>(
      rtk::ecs::Order::SndUpdate, context.eventBus);
  context.scheduler.add<rtk::systems::MovementSystem>(rtk::ecs::Order::Physics);
  context.scheduler.add<rtk::systems::ColliderSystem>(
      rtk::ecs::Order::Physics, _collisionBuffer, 64.f, context.eventBus);
}

void Game::handleConnections(rtk::EngineContext &context) {
  Server::ConnectionEvent connEvent;
  while (_server.pollConnectionEvent(connEvent)) {
    const auto connectionId = connEvent.connectionId;

    if (connEvent.type == Server::ConnectionEvent::Type::Connected) {
      std::cout << "connection" << std::endl;
      if (_players.contains(connectionId))
        continue;

      const std::uint32_t playerNetId = rtype::server::NetworkIdGenerator::getInstance().getNextId();
      const auto entity = context._entityFactory.createNetwork(context.registry, "player", playerNetId);

      auto *transform = context.registry.get_components<component::Transform>().get(entity);
      if (transform) {
        transform->position = {
            150.0f,
            150.0f + static_cast<float>((connEvent.connectionId - 1) % 4) * 100.0f};
      }

      sendExistingEntities(connectionId, context);
      _players[connectionId] = entity;
      broadcastPlayerCreated(connectionId, context);
      std::cout << "[Server] Player connected: " << connectionId << " (NetworkId: " << playerNetId << ")" << std::endl;

    } else if (connEvent.type == Server::ConnectionEvent::Type::Disconnected) {
      std::cout << "[Server] Removing player entity for connection "
                << connEvent.connectionId << std::endl;
      auto it = _players.find(connEvent.connectionId);
      if (it != _players.end()) {
        auto *netEnt = context.registry.get_components<component::NetworkEntity>().get(it->second);
        if (netEnt) {
          broadcastPlayerDestroyed(connectionId, netEnt->id);
        }
        context.registry.kill_entity(it->second);
        _players.erase(it);
      }
    }
  }
}

void Game::handleInputs(rtk::EngineContext &context) {
  Server::InputEvent inputEvent;
  while (_server.pollInput(inputEvent)) {
    auto it = _players.find(inputEvent.connectionId);
    if (it == _players.end())
      continue;

    std::size_t playerEntity = it->second;
    auto *velocity = context.registry.get_components<component::Velocity>().get(playerEntity);
    auto *controllable = context.registry.get_components<component::Controllable>().get(playerEntity);
    auto *transform = context.registry.get_components<component::Transform>().get(playerEntity);

    if (!velocity || !controllable || !transform)
      continue;

    velocity->x = 0.0f;
    velocity->y = 0.0f;

    const auto actions = inputEvent.input.actions;
    if (rtype::protocol::hasAction(actions, rtype::protocol::InputAction::Up)) {
      velocity->y = -controllable->speed;
    }
    if (rtype::protocol::hasAction(actions, rtype::protocol::InputAction::Down)) {
      velocity->y = controllable->speed;
    }
    if (rtype::protocol::hasAction(actions, rtype::protocol::InputAction::Left)) {
      velocity->x = -controllable->speed;
    }
    if (rtype::protocol::hasAction(actions, rtype::protocol::InputAction::Right)) {
      velocity->x = controllable->speed;
    }

    if (velocity->x != 0.0f && velocity->y != 0.0f) {
      velocity->x *= 0.7071f;
      velocity->y *= 0.7071f;
    }

    const bool isShoot = rtype::protocol::hasAction(actions, rtype::protocol::InputAction::Shoot);
    if (isShoot) {
      controllable->isChargingShoot = true;
    } else if (controllable->isChargingShoot) {
      controllable->isChargingShoot = false;

      std::uint32_t bulletNetId = rtype::server::NetworkIdGenerator::getInstance().getNextId();

      auto bulletEntity = context._entityFactory.createNetwork(context.registry, "bullet", bulletNetId);
      auto *bulletTransform = context.registry.get_components<component::Transform>().get(bulletEntity);

      if (bulletTransform) {
        bulletTransform->position = {transform->position.x + 50.0f, transform->position.y};

        protocol::CreateEntityPayload payload;
        payload.entityId = bulletNetId;
        payload.blueprintId = context._entityFactory.getArchetypeID("bullet");
        payload.x = bulletTransform->position.x;
        payload.y = bulletTransform->position.y;

        for (const auto &[connectedClientId, dummy] : _players) {
            _server.sendCreateEntity(connectedClientId, payload);
        }
      }
    }
  }
}

void Game::onUpdate(rtk::EngineContext &context, float) {
  handleConnections(context);
  handleInputs(context);
  handleEntityQueries(context);

  _serverTick++;

  std::cout << "Tick: "<<_serverTick<<std::endl;
  if(_serverTick % 3 == 0){

    sendSnapshots(context);
  }
}

bool Game::shouldContinue() const {
  return _running;
}

float Game::createGate(rtk::EngineContext &context, float startX, bool symmetric) {
  auto gateEntity = context._entityFactory.createNetwork(
      context.registry, "gate", rtype::server::NetworkIdGenerator::getInstance().getNextId());
  auto *transform = context.registry.get_components<component::Transform>().get(gateEntity);
  if (transform) {
    transform->position.x = startX;
  }

  if (symmetric) {
    auto ceilingEntity = context._entityFactory.createNetwork(
        context.registry, "gate", rtype::server::NetworkIdGenerator::getInstance().getNextId());
    auto *ceilTransform = context.registry.get_components<component::Transform>().get(ceilingEntity);
    if (ceilTransform) {
      ceilTransform->position.x = startX;
      ceilTransform->position.y = 0.0f;
    }
  }
  return startX + (192.0f * 2.0f);
}

float Game::createRamp(rtk::EngineContext &context, float startX, bool symmetric) {
  auto rampEntity = context._entityFactory.createNetwork(
      context.registry, "ramp", rtype::server::NetworkIdGenerator::getInstance().getNextId());
  auto *transform = context.registry.get_components<component::Transform>().get(rampEntity);
  if (transform) {
    transform->position.x = startX;
  }

  if (symmetric) {
    auto ceilingEntity = context._entityFactory.createNetwork(
        context.registry, "ramp", rtype::server::NetworkIdGenerator::getInstance().getNextId());
    auto *ceilTransform = context.registry.get_components<component::Transform>().get(ceilingEntity);
    if (ceilTransform) {
      ceilTransform->position.x = startX;
      ceilTransform->position.y = 0.0f;
    }
  }
  return startX + (192.0f * 2.0f);
}

float Game::createReactor(rtk::EngineContext &context, float startX, bool symmetric) {
  auto reactorEntity = context._entityFactory.createNetwork(
      context.registry, "reactor", rtype::server::NetworkIdGenerator::getInstance().getNextId());
  auto *transform = context.registry.get_components<component::Transform>().get(reactorEntity);
  if (transform) {
    transform->position.x = startX;
  }

  if (symmetric) {
    auto ceilingEntity = context._entityFactory.createNetwork(
        context.registry, "reactor", rtype::server::NetworkIdGenerator::getInstance().getNextId());
    auto *ceilTransform = context.registry.get_components<component::Transform>().get(ceilingEntity);
    if (ceilTransform) {
      ceilTransform->position.x = startX;
      ceilTransform->position.y = 0.0f;
    }
  }
  return startX + (113.0f * 2.0f);
}

float Game::createHighGround(rtk::EngineContext &context, float startX, bool symmetric) {
  auto highGroundEntity = context._entityFactory.createNetwork(
      context.registry, "high_ground", rtype::server::NetworkIdGenerator::getInstance().getNextId());
  auto *transform = context.registry.get_components<component::Transform>().get(highGroundEntity);
  if (transform) {
    transform->position.x = startX;
  }

  if (symmetric) {
    auto ceilingEntity = context._entityFactory.createNetwork(
        context.registry, "high_ground", rtype::server::NetworkIdGenerator::getInstance().getNextId());
    auto *ceilTransform = context.registry.get_components<component::Transform>().get(ceilingEntity);
    if (ceilTransform) {
      ceilTransform->position.x = startX;
      ceilTransform->position.y = 0.0f;
    }
  }
  return startX + (64.0f * 2.0f);
}

float Game::createLowGround(rtk::EngineContext &context, float startX, bool symmetric) {
  auto lowGroundEntity = context._entityFactory.createNetwork(
      context.registry, "low_ground", rtype::server::NetworkIdGenerator::getInstance().getNextId());
  auto *transform = context.registry.get_components<component::Transform>().get(lowGroundEntity);
  if (transform) {
    transform->position.x = startX;
  }

  if (symmetric) {
    auto ceilingEntity = context._entityFactory.createNetwork(
        context.registry, "low_ground", rtype::server::NetworkIdGenerator::getInstance().getNextId());
    auto *ceilTransform = context.registry.get_components<component::Transform>().get(ceilingEntity);
    if (ceilTransform) {
      ceilTransform->position.x = startX;
      ceilTransform->position.y = 0.0f;
    }
  }
  return startX + (256.0f * 2.0f);
}

float Game::createMediumGround(rtk::EngineContext &context, float startX, bool symmetric) {
  auto groundEntity = context._entityFactory.createNetwork(
      context.registry, "medium_ground", rtype::server::NetworkIdGenerator::getInstance().getNextId());
  auto *transform = context.registry.get_components<component::Transform>().get(groundEntity);
  if (transform) {
    transform->position.x = startX;
  }

  if (symmetric) {
    auto ceilingEntity = context._entityFactory.createNetwork(
        context.registry, "medium_ground", rtype::server::NetworkIdGenerator::getInstance().getNextId());
    auto *ceilTransform = context.registry.get_components<component::Transform>().get(ceilingEntity);
    if (ceilTransform) {
      ceilTransform->position.x = startX;
      ceilTransform->position.y = 0.0f;
    }
  }
  return startX + (64.0f * 2.0f);
}

void Game::createBackground(rtk::EngineContext &context) {
  float currentX = 0.0f;
  currentX = createMediumGround(context, currentX, false);
  currentX = createLowGround(context, currentX, false);
  currentX = createHighGround(context, currentX, false);
  currentX = createMediumGround(context, currentX, false);
  currentX = createLowGround(context, currentX, false);
  currentX = createHighGround(context, currentX, false);
  currentX = createHighGround(context, currentX, false);
  currentX = createMediumGround(context, currentX, false);
  currentX = createLowGround(context, currentX, false);
  currentX = createMediumGround(context, currentX, false);
  currentX = createReactor(context, currentX, true);
  currentX = createLowGround(context, currentX, true);
  currentX = createMediumGround(context, currentX, true);
  currentX = createHighGround(context, currentX, true);
  currentX = createLowGround(context, currentX, true);
  currentX = createRamp(context, currentX, true);
  currentX = createMediumGround(context, currentX, true);
  currentX = createMediumGround(context, currentX, true);
  currentX = createMediumGround(context, currentX, true);
  currentX = createHighGround(context, currentX, true);
  currentX = createGate(context, currentX, true);
  currentX = createLowGround(context, currentX, true);
  currentX = createMediumGround(context, currentX, true);
  currentX = createHighGround(context, currentX, true);
}

void Game::onUnload(rtk::EngineContext &) {
  _server.stop();
  _players.clear();
  std::cout << "[Server] Game simulation stopped" << std::endl;
}

void Game::sendExistingEntities(std::uint32_t connectionId, rtk::EngineContext &context) {
  auto view = context.registry.view<component::NetworkEntity, component::Transform>();
  auto &transforms = context.registry.get_components<component::Transform>();
  auto &networkEntities = context.registry.get_components<component::NetworkEntity>();

  for (auto entity : view) {
    const auto *transform = transforms.get(entity);
    const auto *netEnt = networkEntities.get(entity);
    if (!transform || !netEnt)
      continue;

    protocol::CreateEntityPayload payload;
    payload.entityId = netEnt->id;
    payload.blueprintId = netEnt->archetypeId;
    payload.x = transform->position.x;
    payload.y = transform->position.y;
    _server.sendCreateEntity(connectionId, payload);
  }
}

void Game::handleEntityQueries(rtk::EngineContext &context) {
  Server::QueryEntityEvent queryEvent;
  while (_server.pollQueryEntity(queryEvent)) {
    auto view = context.registry.view<component::NetworkEntity, component::Transform>();
    auto &transforms = context.registry.get_components<component::Transform>();
    auto &networkEntities = context.registry.get_components<component::NetworkEntity>();

    for (auto entity : view) {
      const auto *netEnt = networkEntities.get(entity);
      if (netEnt && netEnt->id == queryEvent.networkId) {
        const auto *transform = transforms.get(entity);
        if (transform) {
          protocol::CreateEntityPayload payload;
          payload.entityId = netEnt->id;
          payload.blueprintId = netEnt->archetypeId;
          payload.x = transform->position.x;
          payload.y = transform->position.y;
          _server.sendCreateEntity(queryEvent.connectionId, payload);
        }
        break;
      }
    }
  }
}

void Game::broadcastPlayerCreated(std::uint32_t connectionId, rtk::EngineContext &context) {
  const auto it = _players.find(connectionId);
  if (it == _players.end())
    return;

  const auto *transform = context.registry.get_components<component::Transform>().get(it->second);
  const auto *netEnt = context.registry.get_components<component::NetworkEntity>().get(it->second);
  if (!transform || !netEnt)
    return;

  protocol::CreateEntityPayload payload;

  payload.entityId = netEnt->id;
  payload.blueprintId = context._entityFactory.getArchetypeID("player");
  payload.x = transform->position.x;
  payload.y = transform->position.y;
  for (const auto &[playerId, entity] : _players) {
    (void)entity;
    _server.sendCreateEntity(playerId, payload);
  }
}

void Game::broadcastPlayerDestroyed(std::uint32_t connectionId, std::uint32_t networkId) {
  protocol::DestroyEntityPayload payload;
  payload.entityId = networkId;

  for (const auto &[playerId, entity] : _players) {
    (void)entity;
    if (playerId != connectionId)
      _server.sendDestroyEntity(playerId, payload);
  }
}

void Game::sendSnapshots(rtk::EngineContext &context) {
  auto view = context.registry.view<component::NetworkEntity, component::Transform>();
  auto &transforms = context.registry.get_components<component::Transform>();
  auto &networkEntities = context.registry.get_components<component::NetworkEntity>();

  std::cout << "Send Snapshot from game\n";
  protocol::SnapshotPayload snapshot;
  snapshot.serverTick = _serverTick;

  const auto flush = [&]() {
    if (snapshot.entities.empty()){
      std::cout << "SnapShot empty\n";
      return;
    }
    if (_players.empty()){
      std::cout << "No Network entities\n";
    }
    for (const auto &[connectionId, dummy] : _players) {
      _server.sendSnapshot(connectionId, snapshot);
    }
    snapshot.entities.clear();
  };

  for (auto entity : view) {
    const auto *transform = transforms.get(entity);
    const auto *netEnt = networkEntities.get(entity);

    if (!transform || !netEnt)
      continue;

    snapshot.entities.push_back({netEnt->id, transform->position.x, transform->position.y});

    if (snapshot.entities.size() == protocol::SnapshotPayload::MAX_ENTITIES) {
      flush();
    }
  }

  flush();
}

} // namespace rtype::server
