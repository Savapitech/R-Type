#include "game.hpp"

#include <iostream>

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
}

void Game::subscribeEvents(rtk::EngineContext &context) {
  context.eventBus.subscribe<rtk::collision::Collision>(
      [&context](const rtk::collision::Collision &event) {
        auto &colliders = context.registry.get_components<component::AABBCollider>();
        auto *colA = colliders.get(event.firstEntity);
        auto *colB = colliders.get(event.secondEntity);

        if (!colA || !colB) return;

        std::uint32_t collisionPair = colA->layer | colB->layer;
        std::uint32_t bulletVsEnemy =
            Collider::CollisionLayer::PlayerProjectile | Collider::CollisionLayer::Enemy;

        if (collisionPair == bulletVsEnemy) {
          auto &deads = context.registry.get_components<component::Dead>();
          deads.insert_at(event.firstEntity, {});
          deads.insert_at(event.secondEntity, {});
        }
      });
}

void Game::addSystems(rtk::EngineContext &context) {
  context.scheduler.add<rtk::systems::DeathSystem>(rtk::ecs::Order::Update);
  context.scheduler.add<rtk::systems::BulletSystem>(rtk::ecs::Order::Update);
  context.scheduler.add<rtk::systems::EnemySpawnerSystem>(
      rtk::ecs::Order::Update, std::ref(context._entityFactory));
  context.scheduler.add<rtk::systems::EnemyGCSystem>(
      rtk::ecs::Order::SndUpdate, context.eventBus);
  context.scheduler.add<rtk::systems::MovementSystem>(rtk::ecs::Order::Physics);
  context.scheduler.add<rtk::systems::ColliderSystem>(
      rtk::ecs::Order::Physics, _collisionBuffer, 64.f, context.eventBus);
}

void Game::handleConnections(rtk::EngineContext &context) {
  Server::ConnectionEvent connEvent;
  while (_server.pollConnectionEvent(connEvent)) {
    if (connEvent.type == Server::ConnectionEvent::Type::Connected) {
      std::cout << "[Server] Spawning player entity for connection "
                << connEvent.connectionId << std::endl;
      auto playerEntity = context._entityFactory.createFromBlueprint(context.registry, "player");

      context.registry.get_components<component::NetworkEntity>().insert_at(
          playerEntity, component::NetworkEntity{connEvent.connectionId});

      auto *transform = context.registry.get_components<component::Transform>().get(playerEntity);
      if (transform) {
        transform->position = {
            150.0f,
            150.0f + static_cast<float>((connEvent.connectionId - 1) % 4) * 100.0f};
      }

      _players[connEvent.connectionId] = playerEntity;
    } else if (connEvent.type == Server::ConnectionEvent::Type::Disconnected) {
      std::cout << "[Server] Removing player entity for connection "
                << connEvent.connectionId << std::endl;
      auto it = _players.find(connEvent.connectionId);
      if (it != _players.end()) {
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

      auto bulletEntity = context._entityFactory.createFromBlueprint(context.registry, "bullet");
      auto *bulletTransform =
          context.registry.get_components<component::Transform>().get(bulletEntity);
      if (bulletTransform) {
        bulletTransform->position = {transform->position.x + 50.0f, transform->position.y};
      }
    }
  }
}

void Game::onUpdate(rtk::EngineContext &context, float) {
  handleConnections(context);
  handleInputs(context);
}

bool Game::shouldContinue() const {
  return _running;
}

void Game::onUnload(rtk::EngineContext &) {
  _server.stop();
  _players.clear();
  std::cout << "[Server] Game simulation stopped" << std::endl;
}

} // namespace rtype::server
