#include "game.hpp"

#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "GameInstance/GameInstance.hpp"
#include "NetworkIdGenerator.hpp"
#include "Room/Room.hpp"
#include "Room/RoomManager.hpp"

#include "EngineContext.hpp"
#include "Protocol/EntityPayload.hpp"
#include "Protocol/SnapshotPayload.hpp"
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
#include "components/WaveMovement.hpp"

#include "ServerDeathSystem.hpp"
#include "systems/BulletSystem.hpp"
#include "systems/ColliderSystem.hpp"
#include "systems/DeadSystem.hpp"
#include "systems/EnemyGarbageCollectorSystem.hpp"
#include "systems/EnemyShootSystem.hpp"
#include "systems/EnemySpawnerSystem.hpp"
#include "systems/MovementSystem.hpp"
#include "systems/WaveMovementSystem.hpp"

namespace rtype::server {

Game::Game(std::uint16_t port)
    : _server(port) {}

Game::~Game() = default;

void Game::onLoad(rtk::EngineContext &) {

  _server.start();
  _defaultRoomId = _roomManager.createRoom("Default Room");
  GameInstance *instance = createGameInstance(_defaultRoomId, _server);
  
  if (!instance)
    throw std::runtime_error("Failed to create default game Instance.");
  
  instance->start(_server);
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
  context.registry.register_component<component::Health>();
  context.registry.register_component<component::DestroyableTile>();
  context.registry.register_component<component::WaveMovement>();
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
  context._entityFactory.registerBuilder("destroyableTile",
                                         component::destroyableTileFromJson);
  context._entityFactory.registerBuilder("DestroyableTile",
                                         component::destroyableTileFromJson);
  context._entityFactory.registerBuilder("health", component::healthFromJson);
  context._entityFactory.registerBuilder("Health", component::healthFromJson);
  context._entityFactory.registerBuilder(
      "enemyEscapedEvent", rtype::events::enemyEscapedEventFromJson);
  context._entityFactory.registerBuilder(
      "EnemyEscapedEvent", rtype::events::enemyEscapedEventFromJson);
  context._entityFactory.registerBuilder("waveMovement",
                                         component::waveMovementFromJson);
  context._entityFactory.registerBuilder("WaveMovement",
                                         component::waveMovementFromJson);
}

void Game::loadEntityBlueprints(rtk::EngineContext &context) {
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
        std::uint32_t playerVsEnemyProj =
            Collider::CollisionLayer::Player |
            Collider::CollisionLayer::EnemyProjectile;
        std::uint32_t enemyVsEnv = Collider::CollisionLayer::Enemy |
                                   Collider::CollisionLayer::Environment;
        std::uint32_t enemyProjVsEnv =
            Collider::CollisionLayer::EnemyProjectile |
            Collider::CollisionLayer::Environment;

        auto &deads = context.registry.get_components<component::Dead>();

        if (collisionPair == bulletVsEnemy) {
          std::size_t bulletEnt =
              (colA->layer == Collider::CollisionLayer::PlayerProjectile)
                  ? event.firstEntity
                  : event.secondEntity;
          std::size_t enemyEnt =
              (colA->layer == Collider::CollisionLayer::Enemy)
                  ? event.firstEntity
                  : event.secondEntity;

          deads.insert_at(bulletEnt, {});

          auto &enemies = context.registry.get_components<component::Enemy>();
          auto &healths = context.registry.get_components<component::Health>();
          auto &bullets = context.registry.get_components<component::Bullet>();

          auto *bulletComp = bullets.get(bulletEnt);
          const float damage = bulletComp ? bulletComp->damage : 10.0f;

          auto *enemyComp = enemies.get(enemyEnt);
          auto *healthComp = healths.get(enemyEnt);

          if (enemyComp) {
            enemyComp->health -= damage;
            if (enemyComp->health <= 0.0f) {
              deads.insert_at(enemyEnt, {});
            }
          } else if (healthComp) {
            healthComp->current -= damage;
            if (healthComp->current <= 0.0f) {
              deads.insert_at(enemyEnt, {});
            }
          } else {
            deads.insert_at(enemyEnt, {});
          }
        } else if (collisionPair == bulletVsEnv) {
          if (colA->layer == Collider::CollisionLayer::PlayerProjectile) {
            deads.insert_at(event.firstEntity, {});
          } else {
            deads.insert_at(event.secondEntity, {});
          }

          auto &healths = context.registry.get_components<component::Health>();
          auto &bullets = context.registry.get_components<component::Bullet>();

          std::size_t bulletEnt =
              (colA->layer == Collider::CollisionLayer::PlayerProjectile)
                  ? event.firstEntity
                  : event.secondEntity;
          std::size_t tileEnt =
              (colA->layer == Collider::CollisionLayer::Environment)
                  ? event.firstEntity
                  : event.secondEntity;

          auto *bulletComp = bullets.get(bulletEnt);
          auto *healthComp = healths.get(tileEnt);

          if (healthComp && bulletComp) {
            healthComp->current -= bulletComp->damage;
            if (healthComp->current <= 0.0f) {
              deads.insert_at(tileEnt, {});
              std::cout << "[SERVER] Tile destroyed!" << std::endl;
            }
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
        } else if (collisionPair == playerVsEnemyProj) {
          if (colA->layer == Collider::CollisionLayer::Player) {
            deads.insert_at(event.firstEntity, {});
          } else {
            deads.insert_at(event.secondEntity, {});
          }
          deads.insert_at(event.firstEntity, {});
          deads.insert_at(event.secondEntity, {});
          std::cout << "[SERVER] Player hit by enemy bullet!" << std::endl;
        } else if (collisionPair == enemyVsEnv) {
          std::size_t enemyEnt =
              (colA->layer == Collider::CollisionLayer::Enemy)
                  ? event.firstEntity
                  : event.secondEntity;

          auto &transforms =
              context.registry.get_components<component::Transform>();
          const auto *transform = transforms.get(enemyEnt);

          if (transform && transform->position.x <= 1920.0f &&
              transform->position.x >= -50.0f) {
            deads.insert_at(enemyEnt, {});
            std::cout << "[SERVER] Enemy died from environment collision!"
                      << std::endl;
          }
        } else if (collisionPair == enemyProjVsEnv) {
          if (colA->layer == Collider::CollisionLayer::EnemyProjectile) {
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
  context.scheduler.add<rtk::systems::WaveMovementSystem>(
      rtk::ecs::Order::Update);
  context.scheduler.add<rtk::systems::MovementSystem>(rtk::ecs::Order::Physics);
  context.scheduler.add<rtk::systems::ColliderSystem>(
      rtk::ecs::Order::Physics, _collisionBuffer, 64.f, context.eventBus);
  context.scheduler.add<rtk::systems::EnemyShootSystem>(
      rtk::ecs::Order::Update, std::ref(context._entityFactory),
      std::ref(_server), std::ref(_players));
}

void Game::handleConnections(rtk::EngineContext &) {

  Server::ConnectionEvent connEvent;
  while (_server.pollConnectionEvent(connEvent)) {
    const auto connectionId = connEvent.connectionId;

    if (connEvent.type == Server::ConnectionEvent::Type::Connected) {
      if (_roomManager.getRoomId(connectionId))
        continue;

      std::uint32_t roomId = 0;

      for (const auto id : _roomManager.listRooms()) {
        auto *room = _roomManager.getRoom(id);
        if (!room)
          continue;

        if (room->getState() != RoomState::Waiting)
          continue;

        if (room->isFull())
          continue;

        roomId = id;
        break;
      }

      if (roomId == 0) {
        roomId = _roomManager.createRoom("Room " + std::to_string(_roomManager.listRooms().size() + 1));
        std::cout << "[Server] Created room " << roomId << std::endl;
      }

      if (!_roomManager.joinRoom(connectionId, roomId)) {
        std::cerr << "[SERVER] Failed to join room " << roomId << std::endl;
        continue;
      }

      auto *instance = getGameInstance(roomId);

      if (!instance) 
        instance = createGameInstance(roomId, _server);

      if (!instance) {
        _roomManager.leaveRoom(connectionId);
        continue;
      }

      instance->start(_server);

      if (!instance->addConnectedPlayer(connectionId, _server)) {
        _roomManager.leaveRoom(connectionId);
        
        if (!_roomManager.getRoom(roomId))
          destroyGameInstance(roomId);
        
        continue;
      }
      std::cout << "[Server] Player " << connectionId << " joined room " << roomId << std::endl;

    } else if (connEvent.type == Server::ConnectionEvent::Type::Disconnected) {
      auto roomId = _roomManager.getRoomId(connectionId);
      if (!roomId)
        continue;
      
      if (auto* instance = getGameInstance(*roomId))
        instance->removeConnectedPlayer(connectionId, _server);

      _roomManager.leaveRoom(connectionId);

      if (!_roomManager.getRoom(*roomId)) 
        destroyGameInstance(*roomId);
      std::cout << "[Server] Player " << connectionId << " Disconnected" << std::endl;
    }
  }
}

void Game::broadcastGameEnded(rtk::EngineContext &context) {
  auto view = context.registry.view<component::NetworkEntity>();
  auto &networkEntities =
      context.registry.get_components<component::NetworkEntity>();

  for (auto entity : view) {
    const auto *netEnt = networkEntities.get(entity);
    if (!netEnt || netEnt->id == 0)
      continue;

    const auto payload = protocol::destroyEntityPayload(
        _serverTick, netEnt->id, protocol::DestroyReason::GameEnded);
    for (const auto &[playerId, dummy] : _players) {
      _server.sendDestroyEntity(playerId, payload);
    }
  }
}

void Game::handleInputs(rtk::EngineContext &) {
  Server::InputEvent inputEvent;

  while (_server.pollInput(inputEvent)) {
    auto roomId = _roomManager.getRoomId(inputEvent.connectionId);
    if (!roomId)
      continue;

    auto *instance = getGameInstance(*roomId);
    if (!instance)
      continue;
    instance->processInput(inputEvent, _server); 
  }
}

void Game::onUpdate(rtk::EngineContext &context, float dt)
{
  handleConnections(context);
  handleInputs(context);
  handleEntityQueries(context);

  for (auto &[roomId, instance] : _gameInstances) {

    instance->update(dt);

    if (auto *room = _roomManager.getRoom(roomId))
      room->setState(instance->isActive() ? RoomState::Playing : RoomState::Waiting);

    if (instance->isActive()) {
      instance->incrementServerTick();
      if (instance->getServerTick() % 3 == 0)
          instance->sendSnapshots(_server);
    }
  }
}
bool Game::shouldContinue() const { return _running; }


void Game::onUnload(rtk::EngineContext &) {

  for (auto &[roomId, instance] : _gameInstances) {
    instance->stop();
  }

  _gameInstances.clear();
  _server.stop(); 
  std::cout << "[Server] Multiples instances stopped" << std::endl;
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

void Game::handleEntityQueries(rtk::EngineContext &) {

  Server::QueryEntityEvent queryEvent;
  while (_server.pollQueryEntity(queryEvent)) {
    auto roomId = _roomManager.getRoomId(queryEvent.connectionId);
    if (!roomId)
      continue;
    GameInstance *instance = getGameInstance(*roomId);
    if (!instance)
      continue;
    instance->processEntityQuery(queryEvent, _server);
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
  for (const auto &[playerId, _] : _players) 
    _server.sendCreateEntity(playerId, payload);
}

void Game::broadcastPlayerDestroyed(std::uint32_t connectionId,
                                    std::uint32_t networkId) {
  const auto payload = protocol::destroyEntityPayload(
      _serverTick, networkId, protocol::DestroyReason::PlayerDisconnected);

  for (const auto &[playerId, _] : _players) {
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

GameInstance *Game::createGameInstance(std::uint32_t roomId, Server &server) {
  if (_roomManager.getRoom(roomId) == nullptr)
    return nullptr;

  auto it = _gameInstances.find(roomId);
  if (it != _gameInstances.end())
    return it->second.get();

  auto instance = std::make_unique<GameInstance>(roomId, server);
  GameInstance *ptr = instance.get();
  _gameInstances.emplace(roomId, std::move(instance));
  return ptr;
}

GameInstance* Game::getGameInstance(std::uint32_t roomId) {
  auto it = _gameInstances.find(roomId);
  if (it == _gameInstances.end())
    return nullptr;

  return it->second.get();
}

 bool Game::destroyGameInstance(std::uint32_t roomId) {
  auto it = _gameInstances.find(roomId);
  if (it == _gameInstances.end())
    return false;

  it->second->stop();
  _gameInstances.erase(it);
  return true;
}


} // namespace rtype::server
