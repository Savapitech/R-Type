#include "game.hpp"

#include <array>
#include <cmath>
#include <iostream>

#include "NetworkIdGenerator.hpp"

#include "EngineContext.hpp"
#include "Resources/Resources.hpp"
#include "Protocol/EntityPayload.hpp"
#include "Protocol/SnapshotPayload.hpp"
#include "components/AABBCollider.hpp"
#include "components/Dead.hpp"
#include "components/NetworkEntity.hpp"
#include "components/Transform.hpp"
#include "components/Velocity.hpp"

#include "components/Bullet.hpp"
#include "components/Controllable.hpp"
#include "components/Enemy.hpp"
#include "components/EnemyEvent.hpp"

#include "ServerDeathSystem.hpp"
#include "systems/BulletSystem.hpp"
#include "systems/ColliderSystem.hpp"
#include "systems/DeadSystem.hpp"
#include "systems/EnemyGarbageCollectorSystem.hpp"
#include "systems/EnemySpawnerSystem.hpp"
#include "systems/MovementSystem.hpp"

namespace rtype::server {

Game::Game(std::uint16_t port)
    : _serverContext{*this},
      _sceneManager(_serverContext, std::in_place_type<ServerLobbyScene>),
      _server(port) {}

Game::~Game() = default;

void Game::onLoad(rtk::EngineContext &context) {
  registerComponents(context);
  registerBuilders(context);
  loadEntityBlueprints(context);
  subscribeEvents(context);
  addSystems(context);

  _server.start();
  _sceneManager.start(context);
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
  context._entityFactory.registerBuilder("dead", component::deadFromJson);
  context._entityFactory.registerBuilder("Dead", component::deadFromJson);
  context._entityFactory.registerBuilder("networkEntity",
                                         component::networkEntityFromJson);
  context._entityFactory.registerBuilder("NetworkEntity",
                                         component::networkEntityFromJson);
  context._entityFactory.registerBuilder("controllable",
                                         component::controllableFromJson);
  context._entityFactory.registerBuilder("Controllable",
                                         component::controllableFromJson);
  context._entityFactory.registerBuilder("bullet", component::bulletFromJson);
  context._entityFactory.registerBuilder("Bullet", component::bulletFromJson);
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
}

void Game::loadEntityBlueprints(rtk::EngineContext &context) {
  _playerArchetypes[0] = rtype::res::loadBlueprint(context._entityFactory,
      "player_blue", "assets/descriptionEntity/player_blue.json");
  _playerArchetypes[1] = rtype::res::loadBlueprint(context._entityFactory,
      "player_green", "assets/descriptionEntity/player_green.json");
  _playerArchetypes[2] = rtype::res::loadBlueprint(context._entityFactory,
      "player_pink", "assets/descriptionEntity/player_pink.json");
  _playerArchetypes[3] = rtype::res::loadBlueprint(context._entityFactory,
      "player_red", "assets/descriptionEntity/player_red.json");
  rtype::res::loadBlueprint(context._entityFactory,
      "player", "assets/descriptionEntity/player.json");
  rtype::res::loadBlueprint(context._entityFactory,
      "bullet", "assets/descriptionEntity/bullet.json");
  rtype::res::loadBlueprint(context._entityFactory,
      "enemyBidos", "assets/descriptionEntity/enemyBidos.json");
  rtype::res::loadBlueprint(context._entityFactory,
      "gate", "assets/descriptionEntity/gate.json");
  rtype::res::loadBlueprint(context._entityFactory,
      "ramp", "assets/descriptionEntity/ramp.json");
  rtype::res::loadBlueprint(context._entityFactory,
      "reactor", "assets/descriptionEntity/reactor.json");
  rtype::res::loadBlueprint(context._entityFactory,
      "high_ground", "assets/descriptionEntity/high_ground.json");
  rtype::res::loadBlueprint(context._entityFactory,
      "low_ground", "assets/descriptionEntity/low_ground.json");
  rtype::res::loadBlueprint(context._entityFactory,
      "medium_ground", "assets/descriptionEntity/medium_ground.json");
}

void Game::subscribeEvents(rtk::EngineContext &context) {

  context.eventBus.subscribe<rtype::events::EnemySpawnedEvent>(
      [this, &context](const rtype::events::EnemySpawnedEvent &event) {
        std::cout << "[Create]: BIDOS\n";
        const auto payload = protocol::createEntityPayload(
            _serverTick, event.networkId,
            context._entityFactory.getArchetypeID("enemyBidos"), event.x,
            event.y);

        for (const auto &[playerId, entity] : _players) {
          _server.sendCreateEntity(playerId, payload);
        }
      });

  context.eventBus.subscribe<rtk::collision::Collision>(
      [&context](const rtk::collision::Collision &event) {
        auto &colliders =
            context.registry.get_components<component::AABBCollider>();
        auto *colA = colliders.get(event.firstEntity);
        auto *colB = colliders.get(event.secondEntity);

        if (!colA || !colB)
          return;

        std::uint32_t collisionPair = colA->layer | colB->layer;
        std::uint32_t bulletVsEnemy =
            Collider::CollisionLayer::PlayerProjectile |
            Collider::CollisionLayer::Enemy;
        std::uint32_t bulletVsEnv = Collider::CollisionLayer::PlayerProjectile |
                                    Collider::CollisionLayer::Environment;
        std::uint32_t playerVsEnemy =
            Collider::CollisionLayer::Player | Collider::CollisionLayer::Enemy;
        std::uint32_t playerVsEnv = Collider::CollisionLayer::Player |
                                    Collider::CollisionLayer::Environment;

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
  context.scheduler.add<ServerDeathSystem>(
      rtk::ecs::Order::Update, std::ref(_server), std::ref(_players),
      std::cref(_serverTick));
  context.scheduler.add<rtk::systems::BulletSystem>(rtk::ecs::Order::Update);
  context.scheduler.add<rtk::systems::EnemySpawnerSystem>(
      rtk::ecs::Order::Update, std::ref(context._entityFactory),
      std::ref(context.eventBus), [this]() { return _isGameActive; });
  context.scheduler.add<rtk::systems::EnemyGCSystem>(
      rtk::ecs::Order::SecondUpdate, context.eventBus);
  context.scheduler.add<ServerDeathSystem>(
      rtk::ecs::Order::SecondUpdate, std::ref(_server), std::ref(_players),
      std::cref(_serverTick));
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

      sendExistingEntities(connectionId, context);

      std::uint32_t slot = 0;
      std::array<bool, 4> usedSlots = {false, false, false, false};
      for (const auto &[cId, pEntity] : _players) {
        auto *pTransform =
            context.registry.get_components<component::Transform>().get(
                pEntity);
        if (pTransform) {
          int s = static_cast<int>(
              std::lround((pTransform->position.y - 150.0f) / 100.0f));
          if (s >= 0 && s < 4) {
            usedSlots[s] = true;
          }
        }
      }
      for (std::uint32_t i = 0; i < 4; ++i) {
        if (!usedSlots[i]) {
          slot = i;
          break;
        }
      }

      const std::uint32_t playerNetId =
          rtype::server::NetworkIdGenerator::getInstance().getNextId();
      const auto archetypeId = _playerArchetypes[slot % 4];
      const auto entity = context._entityFactory.createNetwork(
          context.registry, archetypeId, playerNetId);

      auto *transform =
          context.registry.get_components<component::Transform>().get(entity);
      if (transform) {
        transform->position = {150.0f,
                               150.0f + static_cast<float>(slot) * 100.0f};
      }

      _players[connectionId] = entity;
      broadcastPlayerCreated(connectionId, context);
      std::cout << "[Server] Player connected: " << connectionId
                << " (NetworkId: " << playerNetId << ", Slot: " << slot << ")"
                << std::endl;

    } else if (connEvent.type == Server::ConnectionEvent::Type::Disconnected) {
      std::cout << "[Server] Removing player entity for connection "
                << connEvent.connectionId << std::endl;
      auto it = _players.find(connEvent.connectionId);
      if (it != _players.end()) {
        auto *netEnt =
            context.registry.get_components<component::NetworkEntity>().get(
                it->second);
        if (netEnt) {
          broadcastPlayerDestroyed(connectionId, netEnt->id);
        }
        context.registry.remove_entity_from_all_pools(it->second);
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
    auto *velocity = context.registry.get_components<component::Velocity>().get(
        playerEntity);
    auto *controllable =
        context.registry.get_components<component::Controllable>().get(
            playerEntity);
    auto *transform =
        context.registry.get_components<component::Transform>().get(
            playerEntity);

    if (!velocity || !controllable || !transform)
      continue;

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

      auto bulletEntity = context._entityFactory.createNetwork(
          context.registry, "bullet", bulletNetId);
      auto *bulletTransform =
          context.registry.get_components<component::Transform>().get(
              bulletEntity);

      if (bulletTransform) {
        bulletTransform->position = {transform->position.x + 50.0f,
                                     transform->position.y};

        const auto payload = protocol::createEntityPayload(
            _serverTick, bulletNetId,
            context._entityFactory.getArchetypeID("bullet"),
            bulletTransform->position.x, bulletTransform->position.y);

        for (const auto &[connectedClientId, dummy] : _players) {
          _server.sendCreateEntity(connectedClientId, payload);
        }
      }
    }
  }
}

void Game::onUpdate(rtk::EngineContext &context, float dt) {
  _sceneManager.update(context, _serverInput, dt);
}

bool Game::shouldContinue() const { return _running; }

float Game::createGate(rtk::EngineContext &context, float startX,
                       bool symmetric) {
  auto gateEntity = context._entityFactory.createNetwork(
      context.registry, "gate",
      rtype::server::NetworkIdGenerator::getInstance().getNextId());
  auto *transform =
      context.registry.get_components<component::Transform>().get(gateEntity);
  if (transform) {
    transform->position.x = startX;
  }

  if (symmetric) {
    auto ceilingEntity = context._entityFactory.createNetwork(
        context.registry, "gate",
        rtype::server::NetworkIdGenerator::getInstance().getNextId());
    auto *ceilTransform =
        context.registry.get_components<component::Transform>().get(
            ceilingEntity);
    if (ceilTransform) {
      ceilTransform->position.x = startX;
      ceilTransform->position.y = 0.0f;
    }
  }
  return startX + (192.0f * 2.0f);
}

float Game::createRamp(rtk::EngineContext &context, float startX,
                       bool symmetric) {
  auto rampEntity = context._entityFactory.createNetwork(
      context.registry, "ramp",
      rtype::server::NetworkIdGenerator::getInstance().getNextId());
  auto *transform =
      context.registry.get_components<component::Transform>().get(rampEntity);
  if (transform) {
    transform->position.x = startX;
  }

  if (symmetric) {
    auto ceilingEntity = context._entityFactory.createNetwork(
        context.registry, "ramp",
        rtype::server::NetworkIdGenerator::getInstance().getNextId());
    auto *ceilTransform =
        context.registry.get_components<component::Transform>().get(
            ceilingEntity);
    if (ceilTransform) {
      ceilTransform->position.x = startX;
      ceilTransform->position.y = 0.0f;
    }
  }
  return startX + (192.0f * 2.0f);
}

float Game::createReactor(rtk::EngineContext &context, float startX,
                          bool symmetric) {
  auto reactorEntity = context._entityFactory.createNetwork(
      context.registry, "reactor",
      rtype::server::NetworkIdGenerator::getInstance().getNextId());
  auto *transform = context.registry.get_components<component::Transform>().get(
      reactorEntity);
  if (transform) {
    transform->position.x = startX;
  }

  if (symmetric) {
    auto ceilingEntity = context._entityFactory.createNetwork(
        context.registry, "reactor",
        rtype::server::NetworkIdGenerator::getInstance().getNextId());
    auto *ceilTransform =
        context.registry.get_components<component::Transform>().get(
            ceilingEntity);
    if (ceilTransform) {
      ceilTransform->position.x = startX;
      ceilTransform->position.y = 0.0f;
    }
  }
  return startX + (113.0f * 2.0f);
}

float Game::createHighGround(rtk::EngineContext &context, float startX,
                             bool symmetric) {
  auto highGroundEntity = context._entityFactory.createNetwork(
      context.registry, "high_ground",
      rtype::server::NetworkIdGenerator::getInstance().getNextId());
  auto *transform = context.registry.get_components<component::Transform>().get(
      highGroundEntity);
  if (transform) {
    transform->position.x = startX;
  }

  if (symmetric) {
    auto ceilingEntity = context._entityFactory.createNetwork(
        context.registry, "high_ground",
        rtype::server::NetworkIdGenerator::getInstance().getNextId());
    auto *ceilTransform =
        context.registry.get_components<component::Transform>().get(
            ceilingEntity);
    if (ceilTransform) {
      ceilTransform->position.x = startX;
      ceilTransform->position.y = 0.0f;
    }
  }
  return startX + (64.0f * 2.0f);
}

float Game::createLowGround(rtk::EngineContext &context, float startX,
                            bool symmetric) {
  auto lowGroundEntity = context._entityFactory.createNetwork(
      context.registry, "low_ground",
      rtype::server::NetworkIdGenerator::getInstance().getNextId());
  auto *transform = context.registry.get_components<component::Transform>().get(
      lowGroundEntity);
  if (transform) {
    transform->position.x = startX;
  }

  if (symmetric) {
    auto ceilingEntity = context._entityFactory.createNetwork(
        context.registry, "low_ground",
        rtype::server::NetworkIdGenerator::getInstance().getNextId());
    auto *ceilTransform =
        context.registry.get_components<component::Transform>().get(
            ceilingEntity);
    if (ceilTransform) {
      ceilTransform->position.x = startX;
      ceilTransform->position.y = 0.0f;
    }
  }
  return startX + (256.0f * 2.0f);
}

float Game::createMediumGround(rtk::EngineContext &context, float startX,
                               bool symmetric) {
  auto groundEntity = context._entityFactory.createNetwork(
      context.registry, "medium_ground",
      rtype::server::NetworkIdGenerator::getInstance().getNextId());
  auto *transform =
      context.registry.get_components<component::Transform>().get(groundEntity);
  if (transform) {
    transform->position.x = startX;
  }

  if (symmetric) {
    auto ceilingEntity = context._entityFactory.createNetwork(
        context.registry, "medium_ground",
        rtype::server::NetworkIdGenerator::getInstance().getNextId());
    auto *ceilTransform =
        context.registry.get_components<component::Transform>().get(
            ceilingEntity);
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

void Game::onUnload(rtk::EngineContext &context) {
  _sceneManager.stop(context);
  _server.stop();
  _players.clear();
  std::cout << "[Server] Game simulation stopped" << std::endl;
}

void Game::sendExistingEntities(std::uint32_t connectionId,
                                rtk::EngineContext &context) {
  auto view =
      context.registry.view<component::NetworkEntity, component::Transform>();
  auto &transforms = context.registry.get_components<component::Transform>();
  auto &networkEntities =
      context.registry.get_components<component::NetworkEntity>();

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
    _server.sendCreateEntity(connectionId, payload);
  }
}

void Game::handleEntityQueries(rtk::EngineContext &context) {
  Server::QueryEntityEvent queryEvent;
  while (_server.pollQueryEntity(queryEvent)) {
    auto view =
        context.registry.view<component::NetworkEntity, component::Transform>();
    auto &transforms = context.registry.get_components<component::Transform>();
    auto &networkEntities =
        context.registry.get_components<component::NetworkEntity>();

    for (auto entity : view) {
      const auto *netEnt = networkEntities.get(entity);
      if (netEnt && netEnt->id == queryEvent.networkId) {
        const auto *transform = transforms.get(entity);
        if (transform) {
          if (!std::isfinite(transform->position.x) ||
              !std::isfinite(transform->position.y) ||
              transform->position.x < protocol::MIN_POSITION ||
              transform->position.x > protocol::MAX_POSITION ||
              transform->position.y < protocol::MIN_POSITION ||
              transform->position.y > protocol::MAX_POSITION) {
            break;
          }
          const auto payload = protocol::createEntityPayload(
              _serverTick, netEnt->id, netEnt->archetypeId,
              transform->position.x, transform->position.y,
              transform->rotation);
          _server.sendCreateEntity(queryEvent.connectionId, payload);
        }
        break;
      }
    }
  }
}

void Game::broadcastPlayerCreated(std::uint32_t connectionId,
                                  rtk::EngineContext &context) {
  const auto it = _players.find(connectionId);
  if (it == _players.end())
    return;

  const auto *transform =
      context.registry.get_components<component::Transform>().get(it->second);
  const auto *netEnt =
      context.registry.get_components<component::NetworkEntity>().get(
          it->second);
  if (!transform || !netEnt)
    return;

  const auto payload = protocol::createEntityPayload(
      _serverTick, netEnt->id, netEnt->archetypeId, transform->position.x,
      transform->position.y, transform->rotation);
  for (const auto &[playerId, entity] : _players) {
    (void)entity;
    _server.sendCreateEntity(playerId, payload);
  }
}

void Game::broadcastPlayerDestroyed(std::uint32_t connectionId,
                                    std::uint32_t networkId) {
  const auto payload = protocol::destroyEntityPayload(
      _serverTick, networkId, protocol::DestroyReason::PlayerDisconnected);

  for (const auto &[playerId, entity] : _players) {
    (void)entity;
    if (playerId != connectionId)
      _server.sendDestroyEntity(playerId, payload);
  }
}

void Game::sendSnapshots(rtk::EngineContext &context) {
  auto view =
      context.registry.view<component::NetworkEntity, component::Transform>();
  auto &transforms = context.registry.get_components<component::Transform>();
  auto &networkEntities =
      context.registry.get_components<component::NetworkEntity>();

  protocol::SnapshotPayload snapshot;
  snapshot.serverTick = _serverTick;

  const auto flush = [&]() {
    if (snapshot.entities.empty()) {
      return;
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

    if (!std::isfinite(transform->position.x) ||
        !std::isfinite(transform->position.y) ||
        transform->position.x < protocol::MIN_POSITION ||
        transform->position.x > protocol::MAX_POSITION ||
        transform->position.y < protocol::MIN_POSITION ||
        transform->position.y > protocol::MAX_POSITION) {
      continue;
    }

    snapshot.entities.push_back(
        {netEnt->id, static_cast<std::uint8_t>(netEnt->archetypeId & 0xFF),
         protocol::encodePosition(transform->position.x),
         protocol::encodePosition(transform->position.y),
         protocol::encodeRotation(transform->rotation)});

    if (snapshot.entities.size() == protocol::SNAPSHOT_MAX_ENTITIES) {
      flush();
    }
  }

  flush();
}

void Game::cleanupOffscreenEnvironment(rtk::EngineContext &context) {
  auto view =
      context.registry.view<component::Transform, component::AABBCollider>();
  auto &transforms = context.registry.get_components<component::Transform>();
  auto &colliders = context.registry.get_components<component::AABBCollider>();
  auto &deads = context.registry.get_components<component::Dead>();

  for (auto entity : view) {
    const auto *col = colliders.get(entity);
    const auto *transform = transforms.get(entity);
    if (!col || !transform)
      continue;

    if ((col->layer & Collider::CollisionLayer::Environment) &&
        (transform->position.x + col->width < -200.0f)) {
      deads.insert_at(entity, {});
    }
  }
}

} // namespace rtype::server
