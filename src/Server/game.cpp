#include "game.hpp"

#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#include "NetworkIdGenerator.hpp"

#include "EngineContext.hpp"
#include "Protocol/EntityPayload.hpp"
#include "Protocol/SnapshotPayload.hpp"
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
  // context.scheduler.add<rtk::systems::DestroyableSystem>(rtk::ecs::Order::SecondUpdate,
  // context.eventBus);
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
        if (it->second != NO_ENTITY) {
          auto *netEnt =
              context.registry.get_components<component::NetworkEntity>().get(
                  it->second);
          if (netEnt) {
            broadcastPlayerDestroyed(connectionId, netEnt->id);
          }
          context.registry.remove_entity_from_all_pools(it->second);
          context.registry.kill_entity(it->second);
        }
        _players.erase(it);
      }
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

float Game::spawnStageSegment(rtk::EngineContext &context,
                              const std::string &blueprint, float startX,
                              float spacing, bool symmetric) {
  auto groundEntity = context._entityFactory.createNetwork(
      context.registry, blueprint,
      rtype::server::NetworkIdGenerator::getInstance().getNextId());
  auto *transform =
      context.registry.get_components<component::Transform>().get(groundEntity);
  if (transform) {
    transform->position.x = startX;
  }

  if (symmetric) {
    auto ceilingEntity = context._entityFactory.createNetwork(
        context.registry, blueprint,
        rtype::server::NetworkIdGenerator::getInstance().getNextId());
    auto *ceilTransform =
        context.registry.get_components<component::Transform>().get(
            ceilingEntity);
    if (ceilTransform) {
      ceilTransform->position.x = startX;
      ceilTransform->position.y = 0.0f;
    }
  }
  return startX + spacing;
}

void Game::loadStage(rtk::EngineContext &context,
                     const std::string &stagePath) {
  std::ifstream file(stagePath);
  if (!file.is_open()) {
    std::cerr << "[Server] Failed to open stage file: " << stagePath
              << std::endl;
    return;
  }

  nlohmann::json stageJson;
  try {
    file >> stageJson;
  } catch (const std::exception &e) {
    std::cerr << "[Server] Failed to parse stage JSON (" << stagePath
              << "): " << e.what() << std::endl;
    return;
  }

  _stageName = stageJson.value("name", "Unnamed Stage");
  _nextStageId = stageJson.value("nextStage", "");
  _stageDuration = stageJson.value("duration", 0.0f);
  _stageTimer = 0.0f;
  _stageObstaclesSpawned = 0;
  _stageActive = true;

  float currentX = stageJson.value("startX", 0.0f);
  if (!stageJson.contains("segments") || !stageJson["segments"].is_array()) {
    std::cerr << "[Server] Stage file missing 'segments' array: " << stagePath
              << std::endl;
    return;
  }

  for (const auto &segment : stageJson["segments"]) {
    const std::string blueprint = segment.value("blueprint", "");
    const bool symmetric = segment.value("symmetric", false);
    const float spacing = segment.value("spacing", 0.0f);
    if (blueprint.empty()) {
      continue;
    }
    currentX =
        spawnStageSegment(context, blueprint, currentX, spacing, symmetric);
    _stageObstaclesSpawned += symmetric ? 2 : 1;
  }
  std::cout << "[Server] Loaded stage from " << stagePath << " (" << _stageName
            << ") with " << _stageObstaclesSpawned << " obstacle entities"
            << std::endl;
}

bool Game::loadStageById(rtk::EngineContext &context,
                         const std::string &stageId,
                         const std::string &manifestPath) {
  std::ifstream file(manifestPath);
  if (!file.is_open()) {
    std::cerr << "[Server] Failed to open stage manifest: " << manifestPath
              << std::endl;
    return false;
  }

  nlohmann::json manifestJson;
  try {
    file >> manifestJson;
  } catch (const std::exception &e) {
    std::cerr << "[Server] Failed to parse stage manifest (" << manifestPath
              << "): " << e.what() << std::endl;
    return false;
  }

  std::string stageFilePath;
  std::string manifestNextStage;
  if (manifestJson.contains("stages") && manifestJson["stages"].is_array()) {
    for (const auto &stageEntry : manifestJson["stages"]) {
      if (stageEntry.value("id", "") == stageId) {
        stageFilePath = stageEntry.value("file", "");
        manifestNextStage = stageEntry.value("nextStage", "");
        break;
      }
    }
  }

  if (stageFilePath.empty()) {
    std::cerr << "[Server] Stage id not found in manifest: " << stageId
              << std::endl;
    return false;
  }

  loadStage(context, stageFilePath);
  _currentStageId = stageId;
  if (_nextStageId.empty() && !manifestNextStage.empty()) {
    _nextStageId = manifestNextStage;
  }
  return true;
}

void Game::loadStageManifest(rtk::EngineContext &context,
                             const std::string &manifestPath) {
  std::ifstream file(manifestPath);
  if (!file.is_open()) {
    std::cerr << "[Server] Failed to open stage manifest: " << manifestPath
              << std::endl;
    return;
  }

  nlohmann::json manifestJson;
  try {
    file >> manifestJson;
  } catch (const std::exception &e) {
    std::cerr << "[Server] Failed to parse stage manifest (" << manifestPath
              << "): " << e.what() << std::endl;
    return;
  }

  const std::string defaultStageId =
      manifestJson.value("defaultStage", "stage1");
  loadStageById(context, defaultStageId, manifestPath);
}

void Game::createBackground(rtk::EngineContext &context) {
  loadStageManifest(context);
}

bool Game::hasActiveEnvironmentEntities(rtk::EngineContext &context) const {
  auto view =
      context.registry.view<component::Transform, component::AABBCollider>();
  auto &colliders = context.registry.get_components<component::AABBCollider>();
  auto &deads = context.registry.get_components<component::Dead>();

  for (auto entity : view) {
    if (deads.get(entity) != nullptr) {
      continue;
    }
    const auto *col = colliders.get(entity);
    if (col && (col->layer & Collider::CollisionLayer::Environment)) {
      return true;
    }
  }
  return false;
}

void Game::clearEnvironmentEntities(rtk::EngineContext &context) {
  auto view =
      context.registry.view<component::Transform, component::AABBCollider>();
  auto &colliders = context.registry.get_components<component::AABBCollider>();
  auto &deads = context.registry.get_components<component::Dead>();

  for (auto entity : view) {
    const auto *col = colliders.get(entity);
    if (col && (col->layer & Collider::CollisionLayer::Environment)) {
      deads.insert_at(entity, {});
    }
  }
}

void Game::clearEnemies(rtk::EngineContext &context) {
  auto view = context.registry.view<component::Enemy>();
  auto &deads = context.registry.get_components<component::Dead>();

  for (auto entity : view) {
    deads.insert_at(entity, {});
  }
}

bool Game::isStageFinished(rtk::EngineContext &context) const {
  if (!_stageActive) {
    return false;
  }
  if (_stageDuration > 0.0f && _stageTimer >= _stageDuration) {
    return true;
  }
  if (_stageObstaclesSpawned > 0 && !hasActiveEnvironmentEntities(context)) {
    return true;
  }
  return false;
}

bool Game::advanceToNextStage(rtk::EngineContext &context) {
  if (_nextStageId.empty()) {
    std::cout << "[Server] No next stage configured after " << _currentStageId
              << std::endl;
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

  for (const auto &[playerId, dummy] : _players) {
    sendExistingEntities(playerId, context);
  }
  return true;
}

void Game::updateStage(rtk::EngineContext &context, float dt) {
  if (!_isGameActive || !_stageActive) {
    return;
  }

  _stageTimer += dt;

  if (isStageFinished(context)) {
    if (!advanceToNextStage(context)) {
      _allStagesCompleted = true;
    }
  }
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
