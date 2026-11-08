#include "GameInstance.hpp"
#include "EngineContext.hpp"
#include "NetworkIdGenerator.hpp"
#include "Protocol/EntityPayload.hpp"
#include "Protocol/ShipMovement.hpp"
#include "Scheduler.hpp"
#include "Server.hpp"
#include "ServerDeathSystem.hpp"
#include "components/AABBCollider.hpp"
#include "components/Bullet.hpp"
#include "components/Controllable.hpp"
#include "components/Dead.hpp"
#include "components/DeathEffect.hpp"
#include "components/DestroyableTile.hpp"
#include "components/Enemy.hpp"
#include "components/EnemyEvent.hpp"
#include "components/Health.hpp"
#include "components/LuaScript.hpp"
#include "components/Nest.hpp"
#include "components/NetworkEntity.hpp"
#include "components/Transform.hpp"
#include "components/Velocity.hpp"
#include "components/WaveMovement.hpp"
#include "components/bossTail.hpp"
#include "components/powerUp.hpp"
#include "systems/BulletSystem.hpp"
#include "systems/ColliderSystem.hpp"
#include "systems/EnemyGarbageCollectorSystem.hpp"
#include "systems/EnemyShootSystem.hpp"
#include "systems/EnemySpawnerSystem.hpp"
#include "systems/LuaScriptSystem.hpp"
#include "systems/MovementSystem.hpp"
#include "systems/NestSystem.hpp"
#include "systems/WaveMovementSystem.hpp"
#include "systems/tailSystem.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <functional>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <utility>

namespace rtype::server {
namespace {
// Systems log when they start and the logger calls std::localtime, which is not
// thread safe. Rooms are created rarely, so they can simply start one by one.
std::mutex systemsStartMutex;
} // namespace

GameInstance::GameInstance(std::uint32_t roomId, Server &server,
                           RoomEventQueue &events)
    : _roomId(roomId), _dependency{*this, server, events},
      _sceneManager(_dependency, std::in_place_type<InstanceLobbyScene>) {}

std::uint32_t GameInstance::getRoomId() const { return _roomId; }

std::uint32_t GameInstance::getServerTick() const { return _serverTick; }

void GameInstance::addPlayer(std::uint32_t connectionId, std::size_t entity) {
  _players.insert_or_assign(connectionId, entity);
}

bool GameInstance::removePlayer(std::uint32_t connectionId) {
  _inputRuns.erase(connectionId);
  _snapshotRates.erase(connectionId);
  _scenerySentTicks.erase(connectionId);
  return _players.erase(connectionId) > 0;
}

void GameInstance::incrementServerTick() { _serverTick++; }

bool GameInstance::hasPlayer(std::uint32_t connectionId) const {
  return _players.find(connectionId) != _players.end();
}

std::size_t GameInstance::getPlayerCount() const { return _players.size(); }

const std::unordered_map<std::uint32_t, std::size_t> &
GameInstance::getPlayers() const {
  return _players;
}

bool GameInstance::isActive() const { return _isActive; }

void GameInstance::setGameActive(bool active) { _isActive = active; }

void GameInstance::resetGameState() {
  _isActive = false;
  _serverTick = 0;
  _inputRuns.clear();
  _scenerySentTicks.clear();
}

void GameInstance::requestGameStart() { _gameStartRequested = true; }

bool GameInstance::consumeGameStartRequest() {
  return std::exchange(_gameStartRequested, false);
}

rtk::EngineContext &GameInstance::getContext() { return _context; }

void GameInstance::start(Server &server) {
  if (_systemsStarted)
    return;

  initialize();
  subscribeEvents(server);
  addSystems(server);
  {
    std::lock_guard<std::mutex> lock(systemsStartMutex);
    _scheduler.start(_registry);
  }
  _sceneManager.start(_context);
  _systemsStarted = true;
}

void GameInstance::update(float dt) {
  if (!_systemsStarted)
    return;

  _sceneManager.update(_context, _sceneInput, dt);
  if (_isActive) {
    for (auto ent : _registry.view<component::Controllable>()) {
      if (auto *ctrl =
              _registry.get_components<component::Controllable>().get(ent)) {
        if (ctrl->weaponTimer > 0.0f) {
          ctrl->weaponTimer -= dt;
          if (ctrl->weaponTimer <= 0.0f) {
            ctrl->weaponTimer = 0.0f;
            ctrl->weaponType = 0;
          }
        }
        if (ctrl->isChargingShoot) {
          ctrl->chargeTimer = std::min(ctrl->chargeTimer + dt, 3.0f);
        }
      }
    }
    _scheduler.run(_registry, dt);
  }
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
  context.registry.register_component<component::LuaScript>();
  context.registry.register_component<component::powerUp>();
  context.registry.register_component<component::DropPowerUp>();
  context.registry.register_component<component::DeathEffect>();
  context.registry.register_component<component::tail>();
  context.registry.register_component<component::Nest>();
}

void GameInstance::registerBuilders(rtk::EngineContext &context) {
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
  context._entityFactory.registerBuilder("luaScript",
                                         component::luaScriptFromJson);
  context._entityFactory.registerBuilder("LuaScript",
                                         component::luaScriptFromJson);
  context._entityFactory.registerBuilder("powerUp", component::powerupFromJson);
  context._entityFactory.registerBuilder("PowerUp", component::powerupFromJson);
  context._entityFactory.registerBuilder("powerup", component::powerupFromJson);
  context._entityFactory.registerBuilder("dropPowerup",
                                         component::dropPowerupFromJson);
  context._entityFactory.registerBuilder("dropPowerUp",
                                         component::dropPowerupFromJson);
  context._entityFactory.registerBuilder("DropPowerUp",
                                         component::dropPowerupFromJson);
  context._entityFactory.registerBuilder("powerup", component::powerupFromJson);
  context._entityFactory.registerBuilder("Powerup", component::powerupFromJson);
  context._entityFactory.registerBuilder("dropPowerup",
                                         component::dropPowerupFromJson);
  context._entityFactory.registerBuilder("DropPowerup",
                                         component::dropPowerupFromJson);
  context._entityFactory.registerBuilder("deathEffect",
                                         component::deathEffectFromJson);
  context._entityFactory.registerBuilder("tail", component::tailFromJson);
  context._entityFactory.registerBuilder("Tail", component::tailFromJson);
  context._entityFactory.registerBuilder("nest", component::nestFromJson);
  context._entityFactory.registerBuilder("Nest", component::nestFromJson);
}

void GameInstance::loadEntityBlueprints(rtk::EngineContext &context) {
  context._entityFactory.loadBlueprintManifest("assets/blueprints.json");

  const auto &factory = context._entityFactory;
  _playerArchetypes[0] = factory.getArchetypeID("player_blue");
  _playerArchetypes[1] = factory.getArchetypeID("player_green");
  _playerArchetypes[2] = factory.getArchetypeID("player_pink");
  _playerArchetypes[3] = factory.getArchetypeID("player_red");

  const char *const sceneryNames[] = {"low_ground", "medium_ground",
                                      "high_ground", "ramp",
                                      "reactor",    "gate"};
  for (std::size_t i = 0; i < _sceneryArchetypes.size(); ++i)
    _sceneryArchetypes[i] = factory.getArchetypeID(sceneryNames[i]);
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

void GameInstance::addSystems(Server &server) {
  if (_systemsRegistered)
    return;

  auto &context = _context;

  context.scheduler.add<ServerDeathSystem>(
      rtk::ecs::Order::Update, std::ref(server), std::ref(_players),
      std::cref(_serverTick), std::ref(context._entityFactory));

  context.scheduler.add<rtk::systems::BulletSystem>(rtk::ecs::Order::Update);

  context.scheduler.add<rtk::systems::EnemySpawnerSystem>(
      rtk::ecs::Order::Update, std::ref(context._entityFactory),
      std::ref(context.eventBus),
      [this]() { return _isActive && _stageRandomSpawns; }, &_stageEnemyPool);

  context.scheduler.add<rtk::systems::EnemyGCSystem>(
      rtk::ecs::Order::SecondUpdate, context.eventBus);

  context.scheduler.add<ServerDeathSystem>(
      rtk::ecs::Order::SecondUpdate, std::ref(server), std::ref(_players),
      std::cref(_serverTick), std::ref(context._entityFactory));

  context.scheduler.add<rtk::systems::MovementSystem>(rtk::ecs::Order::Physics);

  context.scheduler.add<rtk::systems::ColliderSystem>(
      rtk::ecs::Order::Physics, _collisionBuffer, 64.f, context.eventBus);

  context.scheduler.add<rtk::systems::EnemyShootSystem>(
      rtk::ecs::Order::Update, std::ref(context._entityFactory),
      std::ref(server), std::ref(_players));

  context.scheduler.add<rtk::systems::WaveMovementSystem>(
      rtk::ecs::Order::Update);

  context.scheduler.add<rtk::systems::LuaScriptSystem>(rtk::ecs::Order::Update);
  context.scheduler.add<rtk::systems::TailSystem>(rtk::ecs::Order::Update);
  context.scheduler.add<rtk::systems::NestSystem>(
      rtk::ecs::Order::Update, std::ref(context._entityFactory),
      std::ref(context.eventBus));

  _systemsRegistered = true;
}

void GameInstance::subscribeEvents(Server &server) {
  if (_eventsRegistered)
    return;

  _context.eventBus.subscribe<rtype::events::EnemySpawnedEvent>(
      [this, &server](const rtype::events::EnemySpawnedEvent &event) {
        const auto payload = protocol::createEntityPayload(
            _serverTick, event.networkId,
            _context._entityFactory.getArchetypeID(event.enemyBlueprint),
            event.x, event.y);

        for (const auto &[connectionId, entity] : _players)
          server.sendCreateEntity(connectionId, payload);
      });

  _context.eventBus.subscribe<rtk::collision::Collision>(
      [this, &server](const rtk::collision::Collision &event) {
        auto &colliders =
            _context.registry.get_components<component::AABBCollider>();

        auto *colA = colliders.get(event.firstEntity);
        auto *colB = colliders.get(event.secondEntity);

        if (!colA || !colB)
          return;

        const std::uint32_t collisionPair = colA->layer | colB->layer;

        const std::uint32_t bulletVsEnemy =
            Collider::CollisionLayer::PlayerProjectile |
            Collider::CollisionLayer::Enemy;

        const std::uint32_t bulletVsEnv =
            Collider::CollisionLayer::PlayerProjectile |
            Collider::CollisionLayer::Environment;

        const std::uint32_t playerVsEnemy =
            Collider::CollisionLayer::Player | Collider::CollisionLayer::Enemy;

        const std::uint32_t playerVsEnv = Collider::CollisionLayer::Player |
                                          Collider::CollisionLayer::Environment;

        const std::uint32_t playerVsEnemyProj =
            Collider::CollisionLayer::Player |
            Collider::CollisionLayer::EnemyProjectile;

        const std::uint32_t playerVsPowerup = Collider::CollisionLayer::Player |
                                              Collider::CollisionLayer::PowerUp;
        auto &deads = _context.registry.get_components<component::Dead>();

        auto &healths = _context.registry.get_components<component::Health>();

        auto &bullets = _context.registry.get_components<component::Bullet>();

        if (collisionPair == bulletVsEnemy) {
          const bool firstIsBullet =
              colA->layer == Collider::CollisionLayer::PlayerProjectile;
          const std::size_t bulletEnt =
              firstIsBullet ? event.firstEntity : event.secondEntity;
          const std::size_t enemyEnt =
              firstIsBullet ? event.secondEntity : event.firstEntity;

          auto *bulletComp = bullets.get(bulletEnt);
          if (bulletComp && bulletComp->piercing) {
            if (std::find(bulletComp->hitEntities.begin(),
                          bulletComp->hitEntities.end(),
                          enemyEnt) != bulletComp->hitEntities.end()) {
              return;
            }
            bulletComp->hitEntities.push_back(enemyEnt);
          } else {
            deads.insert_at(bulletEnt, {});
          }

          auto &enemies = _context.registry.get_components<component::Enemy>();
          auto *enemyComp = enemies.get(enemyEnt);
          auto *healthComp = healths.get(enemyEnt);
          const bool wasDead = deads.get(enemyEnt) != nullptr;

          const float damage = bulletComp ? bulletComp->damage : 10.0f;
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
          if (!wasDead && deads.get(enemyEnt) && bulletComp &&
              bulletComp->owner != 0) {
            _roundScores[bulletComp->owner] += 100;
            server.sendScoreUpdate(bulletComp->owner,
                                   _roundScores[bulletComp->owner]);
          }

        } else if (collisionPair == bulletVsEnv) {

          const bool firstIsBullet =
              colA->layer == Collider::CollisionLayer::PlayerProjectile;

          const std::size_t bulletEnt =
              firstIsBullet ? event.firstEntity : event.secondEntity;

          const std::size_t tileEnt =
              colA->layer == Collider::CollisionLayer::Environment
                  ? event.firstEntity
                  : event.secondEntity;

          deads.insert_at(bulletEnt, {});

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

          const bool firstIsPlayer =
              colA->layer == Collider::CollisionLayer::Player;
          const std::size_t playerEnt =
              firstIsPlayer ? event.firstEntity : event.secondEntity;
          const std::size_t enemyEnt =
              firstIsPlayer ? event.secondEntity : event.firstEntity;

          deads.insert_at(playerEnt, {});

          auto &enemies = _context.registry.get_components<component::Enemy>();
          auto *enemyComp = enemies.get(enemyEnt);
          if (enemyComp) {
            enemyComp->health -= 100.0f;
            if (enemyComp->health <= 0.0f)
              deads.insert_at(enemyEnt, {});
          }

          std::cout << "[SERVER] Player died from collision!" << std::endl;

        } else if (collisionPair == playerVsEnv) {

          if (colA->layer == Collider::CollisionLayer::Player) {
            deads.insert_at(event.firstEntity, {});
          } else {
            deads.insert_at(event.secondEntity, {});
          }

        } else if (collisionPair == playerVsEnemyProj) {

          deads.insert_at(event.firstEntity, {});
          deads.insert_at(event.secondEntity, {});

          std::cout << "[SERVER] Player hit by enemy bullet!" << std::endl;
        } else if (collisionPair == playerVsPowerup) {
          std::size_t playerEnt =
              (colA->layer == Collider::CollisionLayer::Player)
                  ? event.firstEntity
                  : event.secondEntity;
          std::size_t powerupEnt =
              (colA->layer == Collider::CollisionLayer::PowerUp)
                  ? event.firstEntity
                  : event.secondEntity;

          if (deads.get(powerupEnt) != nullptr)
            return;

          deads.insert_at(powerupEnt, {});

          std::cout << "[SERVER] Player take a powerUp" << std::endl;

          auto *powerupComp =
              _context.registry.get_components<component::powerUp>().get(
                  powerupEnt);
          if (powerupComp) {
            switch (powerupComp->type) {
            case component::powerUpType::Heal: {
              auto *healthComp = healths.get(playerEnt);
              if (healthComp) {
                healthComp->current = std::min(
                    healthComp->current + powerupComp->value, healthComp->max);
              }
              break;
            }
            case component::powerUpType::Shield: {
              auto *healthComp = healths.get(playerEnt);
              if (healthComp) {
                healthComp->current += powerupComp->value;
              }
              break;
            }
            case component::powerUpType::Speed: {
              auto *speedComp =
                  _context.registry.get_components<component::Controllable>()
                      .get(playerEnt);
              if (speedComp) {
                constexpr float MAX_SPEED = 440.0f;
                speedComp->speed =
                    std::min(speedComp->speed + powerupComp->value, MAX_SPEED);
              }
              break;
            }
            case component::powerUpType::WeaponSpread: {
              auto *ctrl =
                  _context.registry.get_components<component::Controllable>()
                      .get(playerEnt);
              if (ctrl) {
                ctrl->weaponType = 1;
                ctrl->weaponTimer =
                    (powerupComp->value > 0.0f) ? powerupComp->value : 5.0f;
                ctrl->weaponMaxDuration = ctrl->weaponTimer;
              }
              break;
            }
            case component::powerUpType::WeaponPlasma: {
              auto *ctrl =
                  _context.registry.get_components<component::Controllable>()
                      .get(playerEnt);
              if (ctrl) {
                ctrl->weaponType = 2;
                ctrl->weaponTimer =
                    (powerupComp->value > 0.0f) ? powerupComp->value : 5.0f;
                ctrl->weaponMaxDuration = ctrl->weaponTimer;
              }
              break;
            }
            }
          }
        }
      });
  _eventsRegistered = true;
}

void GameInstance::processInput(const Server::InputEvent &inputEvent,
                                Server &server) {
  auto it = _players.find(inputEvent.connectionId);
  if (it == _players.end())
    return;

  const std::size_t pEntity = it->second;
  if (pEntity == NO_ENTITY)
    return;
  auto &registry = _context.registry;
  auto *velocity = registry.get_components<component::Velocity>().get(pEntity);
  auto *controllable =
      registry.get_components<component::Controllable>().get(pEntity);
  auto *transform =
      registry.get_components<component::Transform>().get(pEntity);
  if (!velocity || !controllable || !transform)
    return;

  const auto actions = inputEvent.input.actions;
  const auto shipSpeed = rtype::protocol::shipVelocity(actions, controllable->speed);
  velocity->x = shipSpeed.x;
  velocity->y = shipSpeed.y;

  // A repeated Input (redundancy, heartbeat) does not restart the run: the
  // ship has been moving with these actions since the first one.
  auto run = _inputRuns.find(inputEvent.connectionId);
  if (run == _inputRuns.end() || run->second.actions != actions) {
    _inputRuns[inputEvent.connectionId] = {
        actions, inputEvent.input.clientTick.get(), _serverTick};
  }
  const bool isShoot =
      rtype::protocol::hasAction(actions, rtype::protocol::InputAction::Shoot);
  if (isShoot) {
    if (!controllable->isChargingShoot) {
      controllable->isChargingShoot = true;
      controllable->chargeTimer = 0.0f;
    }
  } else if (controllable->isChargingShoot) {
    const float charge = controllable->chargeTimer;
    controllable->isChargingShoot = false;
    controllable->chargeTimer = 0.0f;
    // Creating an entity can reallocate the components: `transform` dangles.
    const auto shooterPosition = transform->position;

    auto spawnSingleProjectile = [&](const std::string &archetypeName,
                                     float xOffset, float yOffset) {
      std::uint32_t bulletNetId =
          rtype::server::NetworkIdGenerator::getInstance().getNextId();

      auto bulletEntity = _context._entityFactory.createNetwork(
          _context.registry, archetypeName, bulletNetId);
      if (auto *bullet =
              _context.registry.get_components<component::Bullet>().get(
                  bulletEntity))
        bullet->owner = inputEvent.connectionId;
      auto *bulletTransform =
          _context.registry.get_components<component::Transform>().get(
              bulletEntity);

      if (bulletTransform) {
        bulletTransform->position = {shooterPosition.x + xOffset,
                                     shooterPosition.y + yOffset};
        const auto payload = protocol::createEntityPayload(
            _serverTick, bulletNetId,
            _context._entityFactory.getArchetypeID(archetypeName),
            bulletTransform->position.x, bulletTransform->position.y);

        for (const auto &[connectedClientId, entity] : _players) {
          server.sendCreateEntity(connectedClientId, payload);
        }
      }
    };

    if (charge >= 2.2f) {
      spawnSingleProjectile("bullet_beam_lvl3", 60.0f, -18.0f);
    } else if (charge >= 1.4f) {
      spawnSingleProjectile("bullet_beam_lvl2", 55.0f, -9.0f);
    } else if (charge >= 0.6f) {
      spawnSingleProjectile("bullet_beam_lvl1", 50.0f, -2.0f);
    } else if (controllable->weaponType == 1) {
      const float angles[3] = {0.0f, -0.22f, 0.22f};
      const float yOffsets[3] = {0.0f, -10.0f, 10.0f};
      const std::uint16_t archId =
          _context._entityFactory.getArchetypeID("bullet");

      for (int i = 0; i < 3; ++i) {
        std::uint32_t bulletNetId =
            rtype::server::NetworkIdGenerator::getInstance().getNextId();

        auto bulletEntity = _context._entityFactory.createNetwork(
            _context.registry, "bullet", bulletNetId);
        if (auto *bullet =
                _context.registry.get_components<component::Bullet>().get(
                    bulletEntity))
          bullet->owner = inputEvent.connectionId;
        auto *bulletTransform =
            _context.registry.get_components<component::Transform>().get(
                bulletEntity);
        auto *bulletVel =
            _context.registry.get_components<component::Velocity>().get(
                bulletEntity);

        if (bulletTransform) {
          bulletTransform->position = {shooterPosition.x + 50.0f,
                                       shooterPosition.y + yOffsets[i]};
          bulletTransform->rotation = angles[i];

          if (bulletVel) {
            constexpr float speed = 800.0f;
            bulletVel->x = speed * std::cos(angles[i]);
            bulletVel->y = speed * std::sin(angles[i]);
          }

          const auto payload = protocol::createEntityPayload(
              _serverTick, bulletNetId, archId, bulletTransform->position.x,
              bulletTransform->position.y, angles[i]);

          for (const auto &[connectedClientId, entity] : _players) {
            server.sendCreateEntity(connectedClientId, payload);
          }
        }
      }
    } else if (controllable->weaponType == 2) {
      spawnSingleProjectile("bullet_plasma", 55.0f, 0.0f);
    } else {
      spawnSingleProjectile("bullet", 50.0f, 0.0f);
    }
  }
}

bool GameInstance::addConnectedPlayer(std::uint32_t connectionId,
                                      Server &server) {
  if (hasPlayer(connectionId))
    return false;

  if (_players.size() >= 4)
    return false;

  initialize();
  auto &registry = _context.registry;
  std::array<bool, 4> slots = {false, false, false, false};
  for (const auto &[_, entity] : _players) {

    auto *transform =
        registry.get_components<component::Transform>().get(entity);
    if (!transform)
      continue;

    int slot =
        static_cast<int>(std::lround((transform->position.y - 150.f) / 100.f));
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
  const auto entity =
      _context._entityFactory.createNetwork(registry, archetypeId, networkId);

  auto *transform = registry.get_components<component::Transform>().get(entity);
  if (!transform)
    return false;
  transform->position = {150.0f, 150.0f + static_cast<float>(slot) * 100.0f};
  _players.emplace(connectionId, entity);
  const auto payload = protocol::createEntityPayload(
      _serverTick, networkId, archetypeId, transform->position.x,
      transform->position.y, transform->rotation);
  for (const auto &[clientId, _] : _players) {
    server.sendCreateEntity(clientId, payload);
  }

  std::cout << "[GameInstance " << _roomId << "] Player " << connectionId
            << " created (NetworkId: " << networkId << ", Slot: " << slot << ")"
            << std::endl;
  return true;
}

void GameInstance::sendExistingEntities(std::uint32_t connectionId,
                                        Server &server) {
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

bool GameInstance::removeConnectedPlayer(std::uint32_t connectionId,
                                         Server &server) {
  auto it = _players.find(connectionId);
  if (it == _players.end())
    return false;

  const auto entity = it->second;
  if (entity == NO_ENTITY) {
    _players.erase(it);
    _inputRuns.erase(connectionId);
    _snapshotRates.erase(connectionId);
    _scenerySentTicks.erase(connectionId);
    return true;
  }

  auto &registry = _context.registry;
  auto *netEnt =
      registry.get_components<component::NetworkEntity>().get(entity);

  if (netEnt) {
    const auto payload = protocol::destroyEntityPayload(
        _serverTick, netEnt->id, protocol::DestroyReason::Unknown);
    for (const auto &[clientId, _] : _players) {
      if (clientId == connectionId)
        continue;
      server.sendDestroyEntity(clientId, payload);
    }
  }

  registry.remove_entity_from_all_pools(entity);
  registry.kill_entity(entity);
  _players.erase(it);
  _inputRuns.erase(connectionId);
  _snapshotRates.erase(connectionId);
  _scenerySentTicks.erase(connectionId);
  return true;
}

namespace {
constexpr std::uint32_t RATE_CHECK_PERIOD = 5;
/// Growth of the smallest RTT, in ms, above which a link counts as congested.
constexpr float CONGESTION_MARGIN_MS = 80.0f;
constexpr std::uint32_t CONGESTED_CHECKS_TO_SLOW = 2;
/// Growth above which the queue is already long: go to the slowest rate at once.
constexpr float SEVERE_CONGESTION_MS = 300.0f;
constexpr std::uint32_t SCENERY_REFRESH_BASE_TICKS = 90;
constexpr std::uint32_t SCENERY_REFRESH_SPREAD = 20;
constexpr std::uint32_t SCENERY_REFRESH_SPREAD_TICKS = 3;
constexpr std::uint32_t SCENERY_PRUNE_PERIOD = 100;
constexpr std::size_t SCENERY_PRUNE_SLACK = 64;
/// Slowest rate: one snapshot out of this many (5 per second).
constexpr std::uint32_t MAX_SNAPSHOT_DIVISOR = 4;
/// Checks without congestion before the rate goes up one step (6 s): trying a
/// faster rate on a link that cannot carry it fills the queue again.
constexpr std::uint32_t CLEAN_CHECKS_TO_SPEED_UP = 24;
} // namespace

std::uint32_t GameInstance::sceneryRefreshTicks(std::uint32_t networkId) {
  return SCENERY_REFRESH_BASE_TICKS +
         (networkId % SCENERY_REFRESH_SPREAD) * SCENERY_REFRESH_SPREAD_TICKS;
}

void GameInstance::updateSnapshotRates(Server &server) {
  const auto metrics = server.getAllClientMetrics();
  for (const auto &[connectionId, entity] : _players) {
    (void)entity;
    const auto client = metrics.find(connectionId);
    if (client == metrics.end() || client->second.windowMinRtt <= 0.0f)
      continue;

    SnapshotRate &rate = _snapshotRates[connectionId];
    const float rtt = client->second.windowMinRtt;
    if (rate.baseRtt == 0.0f || rtt < rate.baseRtt)
      rate.baseRtt = rtt;

    const float queueDelay = rtt - rate.baseRtt;
    if (queueDelay > CONGESTION_MARGIN_MS) {
      rate.cleanChecks = 0;
      ++rate.congestedChecks;
      if (queueDelay > SEVERE_CONGESTION_MS) {
        rate.divisor = MAX_SNAPSHOT_DIVISOR;
      } else if (rate.congestedChecks >= CONGESTED_CHECKS_TO_SLOW &&
                 rate.divisor < MAX_SNAPSHOT_DIVISOR) {
        ++rate.divisor;
        rate.congestedChecks = 0;
      }
    } else {
      rate.congestedChecks = 0;
      if (++rate.cleanChecks >= CLEAN_CHECKS_TO_SPEED_UP && rate.divisor > 1) {
        --rate.divisor;
        rate.cleanChecks = 0;
      }
    }
  }
}

std::uint32_t GameInstance::ackedInputTick(std::uint32_t connectionId) const {
  const auto run = _inputRuns.find(connectionId);
  if (run == _inputRuns.end())
    return 0;
  return run->second.clientTick + (_serverTick - run->second.serverTick);
}

void GameInstance::sendSnapshots(Server &server) {
  auto &registry = _context.registry;
  auto view = registry.view<component::NetworkEntity, component::Transform>();
  auto &transforms = registry.get_components<component::Transform>();
  auto &networkEntities = registry.get_components<component::NetworkEntity>();

  ++_snapshotCount;
  if (_snapshotCount % RATE_CHECK_PERIOD == 0)
    updateSnapshotRates(server);

  struct Candidate {
    protocol::SnapshotEntityState state;
    bool scenery;
  };
  std::vector<Candidate> candidates;
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
        transform->position.y > protocol::MAX_POSITION)
      continue;

    candidates.push_back(
        {{netEnt->id, static_cast<std::uint8_t>(netEnt->archetypeId & 0xFF),
          protocol::encodePosition(transform->position.x),
          protocol::encodePosition(transform->position.y),
          protocol::encodeRotation(transform->rotation)},
         isScenery(netEnt->archetypeId)});
  }

  if (_snapshotCount % SCENERY_PRUNE_PERIOD == 0)
    pruneScenerySentTicks(candidates.size());

  for (const auto &[connectionId, entity] : _players) {
    const auto rate = _snapshotRates.find(connectionId);
    if (rate != _snapshotRates.end() &&
        _snapshotCount % rate->second.divisor != 0)
      continue;

    protocol::SnapshotPayload snapshot;
    snapshot.serverTick = _serverTick;
    snapshot.ackedInputTick = ackedInputTick(connectionId);
    if (entity != NO_ENTITY) {
      if (const auto *own = networkEntities.get(entity))
        snapshot.ownNetworkId = own->id;
    }

    auto &sentTicks = _scenerySentTicks[connectionId];
    for (const auto &candidate : candidates) {
      const std::uint32_t id = candidate.state.networkId.get();
      if (candidate.scenery) {
        const auto sent = sentTicks.find(id);
        if (sent != sentTicks.end() &&
            _serverTick - sent->second < sceneryRefreshTicks(id))
          continue;
        sentTicks[id] = _serverTick;
      }
      snapshot.entities.push_back(candidate.state);
      if (snapshot.entities.size() == protocol::SNAPSHOT_MAX_ENTITIES) {
        server.sendSnapshot(connectionId, snapshot);
        snapshot.entities.clear();
      }
    }
    if (!snapshot.entities.empty())
      server.sendSnapshot(connectionId, snapshot);
  }
}

bool GameInstance::isScenery(std::uint16_t archetypeId) const {
  return std::find(_sceneryArchetypes.begin(), _sceneryArchetypes.end(),
                   archetypeId) != _sceneryArchetypes.end();
}

void GameInstance::pruneScenerySentTicks(std::size_t aliveEntities) {
  for (auto &[connectionId, sentTicks] : _scenerySentTicks) {
    (void)connectionId;
    if (sentTicks.size() > aliveEntities + SCENERY_PRUNE_SLACK)
      sentTicks.clear();
  }
}

void GameInstance::processEntityQuery(
    const Server::QueryEntityEvent &queryEvent, Server &server) {
  if (!hasPlayer(queryEvent.connectionId))
    return;

  auto &registry = _context.registry;
  auto view = registry.view<component::NetworkEntity, component::Transform>();

  auto &transforms = registry.get_components<component::Transform>();
  auto &networkEntities = registry.get_components<component::NetworkEntity>();

  for (auto entity : view) {
    const auto *netEnt = networkEntities.get(entity);
    if (!netEnt || netEnt->id != queryEvent.networkId)
      continue;

    const auto *transform = transforms.get(entity);
    if (!transform)
      break;

    if (!std::isfinite(transform->position.x) ||
        !std::isfinite(transform->position.y) ||
        transform->position.x < protocol::MIN_POSITION ||
        transform->position.x > protocol::MAX_POSITION ||
        transform->position.y < protocol::MIN_POSITION ||
        transform->position.y > protocol::MAX_POSITION) {
      break;
    }

    const auto payload = protocol::createEntityPayload(
        _serverTick, netEnt->id, netEnt->archetypeId, transform->position.x,
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
  auto *transform =
      _context.registry.get_components<component::Transform>().get(
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
  auto *transform =
      _context.registry.get_components<component::Transform>().get(
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
  auto *transform =
      _context.registry.get_components<component::Transform>().get(
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
      _context.registry.get_components<component::Transform>().get(
          groundEntity);
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

void GameInstance::createBackground() {
  _currentStageId.clear();
  _nextStageId.clear();
  _stageName.clear();

  _stageTimer = 0.0f;
  _stageDuration = 0.0f;
  _stageObstaclesSpawned = 0;

  _stageActive = false;
  _allStagesCompleted = false;
  _bossSpawned = false;

  loadStageManifest(_context);
}

void GameInstance::endGame(Server &server) {
  auto &registry = _context.registry;
  auto &netEntities = registry.get_components<component::NetworkEntity>();
  for (auto entity : registry.view<component::NetworkEntity>()) {
    const auto *netEnt = netEntities.get(entity);
    if (!netEnt || netEnt->id == 0)
      continue;

    const auto payload = protocol::destroyEntityPayload(
        _serverTick, netEnt->id, protocol::DestroyReason::GameEnded);

    for (const auto &[connectionId, _] : _players)
      server.sendDestroyEntity(connectionId, payload);
  }
  std::vector<std::size_t> entities;

  for (auto entity : registry.view<component::Transform>())
    entities.push_back(entity);

  for (auto entity : entities) {
    registry.remove_entity_from_all_pools(entity);
    registry.kill_entity(entity);
  }
  registry.flush();

  for (auto &[_, entity] : _players)
    entity = NO_ENTITY;
  _isActive = false;
  _serverTick = 0;
  _inputRuns.clear();
  _scenerySentTicks.clear();
}

void GameInstance::respawnPlayers() {
  auto &registry = _context.registry;
  std::vector<std::uint32_t> connectionIds;

  for (auto &[connectionId, _] : _players) {
    connectionIds.push_back(connectionId);
  }

  std::sort(connectionIds.begin(), connectionIds.end());
  std::size_t slot = 0;

  for (const auto &connectionId : connectionIds) {
    if (slot >= _playerArchetypes.size())
      break;
    const auto netId = NetworkIdGenerator::getInstance().getNextId();
    const auto archId = _playerArchetypes[slot];
    const auto entity =
        _context._entityFactory.createNetwork(registry, archId, netId);

    auto *transform =
        registry.get_components<component::Transform>().get(entity);
    if (!transform) {
      registry.remove_entity_from_all_pools(entity);
      registry.kill_entity(entity);
      ++slot;
      continue;
    }

    transform->position = {150.0f, 150.0f + static_cast<float>(slot) * 100.0f};
    _players[connectionId] = entity;
    ++slot;
  }
}

bool GameInstance::hasAlivePlayers() {
  auto &transform = _registry.get_components<component::Transform>();

  for (const auto &[_, entity] : _players) {
    if (entity == NO_ENTITY)
      continue;
    if (transform.get(entity) != nullptr)
      return true;
  }
  return false;
}

float GameInstance::spawnStageSegment(rtk::EngineContext &context,
                                      const std::string &blueprint,
                                      float startX, float spacing,
                                      bool symmetric) {
  auto groundEntity = context._entityFactory.createNetwork(
      context.registry, blueprint,
      NetworkIdGenerator::getInstance().getNextId());

  auto *transform =
      context.registry.get_components<component::Transform>().get(groundEntity);

  if (transform)
    transform->position.x = startX;

  if (symmetric) {
    auto ceilingEntity = context._entityFactory.createNetwork(
        context.registry, blueprint,
        NetworkIdGenerator::getInstance().getNextId());
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

void GameInstance::loadStage(rtk::EngineContext &context,
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
  _bossSpawned = false;
  _bossNetId = 0;

  _stageRandomSpawns = stageJson.value("randomSpawns", true);
  _stageEnemyPool = stageJson.value(
      "enemies", std::string(rtk::systems::EnemySpawnerSystem::defaultPool));
  if (!rtk::systems::EnemySpawnerSystem::hasPool(_stageEnemyPool)) {
    std::cerr << "[Server] Unknown enemy pool '" << _stageEnemyPool << "' in "
              << stagePath << ", falling back to '"
              << rtk::systems::EnemySpawnerSystem::defaultPool << "'"
              << std::endl;
    _stageEnemyPool = rtk::systems::EnemySpawnerSystem::defaultPool;
  }
  _stageSpawns.clear();
  _nextStageSpawn = 0;
  if (stageJson.contains("spawns") && stageJson["spawns"].is_array()) {
    for (const auto &spawn : stageJson["spawns"]) {
      StageSpawn entry;
      entry.blueprint = spawn.value("blueprint", "");
      if (entry.blueprint.empty() ||
          !context._entityFactory.hasBlueprint(entry.blueprint)) {
        std::cerr << "[Server] Ignoring spawn with unknown blueprint '"
                  << entry.blueprint << "' in " << stagePath << std::endl;
        continue;
      }
      entry.time = spawn.value("time", 0.0f);
      entry.x = spawn.value("x", 1300.0f);
      entry.y = spawn.value("y", 0.0f);
      if (spawn.contains("wave") && spawn["wave"].is_object()) {
        entry.hasWave = true;
        entry.wave.amplitude = spawn["wave"].value("amplitude", 120.0f);
        entry.wave.frequency = spawn["wave"].value("frequency", 2.5f);
      }
      _stageSpawns.push_back(std::move(entry));
    }
    std::stable_sort(_stageSpawns.begin(), _stageSpawns.end(),
                     [](const StageSpawn &a, const StageSpawn &b) {
                       return a.time < b.time;
                     });
  }

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

    if (blueprint.empty())
      continue;

    currentX =
        spawnStageSegment(context, blueprint, currentX, spacing, symmetric);
    _stageObstaclesSpawned += symmetric ? 2 : 1;
  }

  std::cout << "[Server] Loaded stage from " << stagePath << " (" << _stageName
            << ") with " << _stageObstaclesSpawned << " obstacle entities"
            << std::endl;
}

bool GameInstance::loadStageById(rtk::EngineContext &context,
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
  if (_nextStageId.empty() && !manifestNextStage.empty())
    _nextStageId = manifestNextStage;
  return true;
}

void GameInstance::loadStageManifest(rtk::EngineContext &context,
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

bool GameInstance::hasActiveEnvironmentEntities(
    rtk::EngineContext &context) const {
  auto view =
      context.registry.view<component::Transform, component::AABBCollider>();
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
  auto view =
      context.registry.view<component::Transform, component::AABBCollider>();
  auto &colliders = context.registry.get_components<component::AABBCollider>();
  auto &deads = context.registry.get_components<component::Dead>();

  for (auto entity : view) {
    const auto *col = colliders.get(entity);

    if (col && ((col->layer & Collider::CollisionLayer::Environment) ||
                (col->layer & Collider::CollisionLayer::BossRoom))) {
      deads.insert_at(entity, {});
    }
  }
}

bool GameInstance::isBossDefeated(rtk::EngineContext &context) const {
  auto view = context.registry.view<component::NetworkEntity>();
  auto &netEntities =
      context.registry.get_components<component::NetworkEntity>();
  auto &deads = context.registry.get_components<component::Dead>();

  for (auto entity : view) {
    const auto *net = netEntities.get(entity);
    if (net && net->id == _bossNetId) {
      if (deads.get(entity) != nullptr) {
        return true;
      } else {
        return false;
      }
    }
  }
  return true;
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

  if (_bossSpawned)
    return isBossDefeated(context);

  if (_stageDuration > 0.0f && _stageTimer >= _stageDuration)
    return true;

  if (_nextStageSpawn < _stageSpawns.size())
    return false;

  if (_stageObstaclesSpawned > 0 && !hasActiveEnvironmentEntities(context))
    return true;

  // Terrain-less stage driven only by scripted spawns: done once they have all
  // been played and the last enemy is gone.
  if (_stageObstaclesSpawned == 0 && !_stageSpawns.empty() &&
      !_stageRandomSpawns) {
    auto enemies = context.registry.view<component::Enemy>();
    return !(enemies.begin() != enemies.end());
  }
  return false;
}

bool GameInstance::advanceToNextStage(rtk::EngineContext &context) {
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
  for (const auto &[playerId, _] : _players) {
    sendExistingEntities(playerId, _dependency.server);
  }
  return true;
}

void GameInstance::spawnDueStageEntities(rtk::EngineContext &context) {
  while (_nextStageSpawn < _stageSpawns.size() &&
         _stageSpawns[_nextStageSpawn].time <= _stageTimer) {
    const StageSpawn &spawn = _stageSpawns[_nextStageSpawn++];
    const std::uint32_t netId = NetworkIdGenerator::getInstance().getNextId();
    const auto entity = context._entityFactory.createNetwork(
        context.registry, spawn.blueprint, netId);

    auto *transform =
        context.registry.get_components<component::Transform>().get(entity);
    if (!transform)
      continue;
    transform->position.x = spawn.x;
    transform->position.y = spawn.y;

    auto *wave =
        context.registry.get_components<component::WaveMovement>().get(entity);
    if (wave) {
      wave->baseY = spawn.y;
      wave->time = 0.0f;
      if (spawn.hasWave) {
        wave->amplitude = spawn.wave.amplitude;
        wave->frequency = spawn.wave.frequency;
      }
    }

    context.eventBus.publish(rtype::events::EnemySpawnedEvent{
        netId, spawn.x, spawn.y, spawn.blueprint});
  }
}

void GameInstance::updateStage(float dt) {
  auto &context = _context;
  if (!_isActive || !_stageActive)
    return;
  _stageTimer += dt;
  spawnDueStageEntities(context);

  auto viewTransCol =
      context.registry.view<component::Transform, component::AABBCollider>();
  auto &transforms = context.registry.get_components<component::Transform>();
  auto &colliders = context.registry.get_components<component::AABBCollider>();
  auto &velocities = context.registry.get_components<component::Velocity>();
  auto &deads = context.registry.get_components<component::Dead>();
  bool stopScrolling = false;
  for (auto entity : viewTransCol) {
    auto *col = colliders.get(entity);
    auto *transform = transforms.get(entity);

    if (deads.get(entity) != nullptr)
      continue;
    if (col && transform && (col->layer & Collider::CollisionLayer::BossRoom)) {
      if (transform->position.x <= 0.0f) {
        transform->position.x = 0.0f;
        stopScrolling = true;
        break;
      }
    }
  }
  if (stopScrolling) {
    auto viewVelCol =
        context.registry.view<component::Velocity, component::AABBCollider>();
    for (auto entity : viewVelCol) {
      auto *col = colliders.get(entity);
      auto *vel = velocities.get(entity);
      if (col && vel &&
          ((col->layer & Collider::CollisionLayer::Environment) ||
           (col->layer & Collider::CollisionLayer::BossRoom))) {
        vel->x = 0.0f;
      }
    }

    if (!_bossSpawned) {
      _stageRandomSpawns = false;
      std::uint32_t bodyNetId = NetworkIdGenerator::getInstance().getNextId();
      auto bossEntity = context._entityFactory.createNetwork(
          context.registry, "dobkeratops_body", bodyNetId);
      auto *bossTransform = transforms.get(bossEntity);
      auto *bossVelocity = velocities.get(bossEntity);

      if (bossTransform) {
        constexpr float screenH = 720.0f;
        const float bossH = 204.0f * bossTransform->scale.y;
        const float bossW = 155.0f * bossTransform->scale.x;
        bossTransform->position.x = std::round(1000.0f - (bossW / 2.0f));
        bossTransform->position.y = std::round((screenH - bossH) / 2.0f);

        _eventBus.publish(rtype::events::EnemySpawnedEvent{
            bodyNetId, bossTransform->position.x, bossTransform->position.y,
            "dobkeratops_body"});

        const auto bossPos = bossTransform->position;
        const auto bossScale = bossTransform->scale;

        auto *bossTail =
            context.registry.get_components<component::tail>().get(bossEntity);
        if (bossTail) {
          const float tipScreenX = bossPos.x + bossTail->tipX * bossScale.x;
          const float tipScreenY = bossPos.y + bossTail->tipY * bossScale.y;

          bossTail->_segments.clear();
          for (const auto &segmentBp : bossTail->_segmentsBluePrint) {
            std::uint32_t segmentNetId =
                NetworkIdGenerator::getInstance().getNextId();
            auto segmentEntity = context._entityFactory.createNetwork(
                context.registry, segmentBp, segmentNetId);
            bossTail->_segments.push_back(segmentEntity);

            if (auto *segTransform = transforms.get(segmentEntity)) {
              segTransform->position = {tipScreenX, tipScreenY};
              _eventBus.publish(rtype::events::EnemySpawnedEvent{
                  segmentNetId, segTransform->position.x,
                  segTransform->position.y, segmentBp});
            }
          }
        }
        std::uint32_t hitboxNetId =
            NetworkIdGenerator::getInstance().getNextId();
        auto bottomHitbox = context._entityFactory.createNetwork(
            context.registry, "dobkeratops_bottom_hitbox", hitboxNetId);
        if (auto *hitboxTransform = transforms.get(bottomHitbox)) {
          hitboxTransform->position = bossPos;
          _eventBus.publish(rtype::events::EnemySpawnedEvent{
              hitboxNetId, hitboxTransform->position.x,
              hitboxTransform->position.y, "dobkeratops_bottom_hitbox"});
        }
        std::uint32_t alienNetId =
            NetworkIdGenerator::getInstance().getNextId();
        auto alienEntity = context._entityFactory.createNetwork(
            context.registry, "dobkeratops_alien", alienNetId);
        auto *alienTransform = transforms.get(alienEntity);
        auto *alienVelocity = velocities.get(alienEntity);
        if (alienTransform) {
          alienTransform->position.x = bossPos.x + 72.0f * bossScale.x;
          alienTransform->position.y = bossPos.y + 94.0f * bossScale.y;
          _eventBus.publish(rtype::events::EnemySpawnedEvent{
              alienNetId, alienTransform->position.x,
              alienTransform->position.y, "dobkeratops_alien"});
        }
        _bossNetId = alienNetId;
      }
      _bossSpawned = true;
    }
  }
  if (isStageFinished(context)) {
    if (!advanceToNextStage(context)) {
      _allStagesCompleted = true;
    }
  }
}

} // namespace rtype::server
