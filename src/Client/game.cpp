#include "game.hpp"

#include "EngineContext.hpp"
#include "NetworkClient.hpp"
#include "components/Transform.hpp"
#include "components/Velocity.hpp"
#include "components/AABBCollider.hpp"
#include "components/Animation.hpp"
#include "systems/AnimatorSystem.hpp"
#include "components/Controllable.hpp"
#include "components/EnemyEvent.hpp"
#include "components/Enemy.hpp"
#include "components/Dead.hpp"
#include "components/NetworkEntity.hpp"
#include "components/SoundRequest.hpp"
#include "components/Bullet.hpp"

#include "systems/AnimationSystem.hpp"
#include "systems/AnimatorSystem.hpp"
#include "systems/ColliderSystem.hpp"
#include "systems/DeadSystem.hpp"
#include "systems/DebugCollisionSystem.hpp"
#include "systems/InputSystem.hpp"
#include "systems/MovementSystem.hpp"
#include "systems/RenderConcept.hpp"
#include "systems/RenderSystem.hpp"
#include "systems/MovementSystem.hpp"
#include "systems/DeadSystem.hpp"
#include "systems/PlayerSystem.hpp"
#include "systems/BulletSystem.hpp"

#include "components/Controllable.hpp"
#include "components/Dead.hpp"
#include "components/Enemy.hpp"
#include "components/EnemyEvent.hpp"
#include "components/NetworkEntity.hpp"
#include "systems/MovementSystem.hpp"
#include "systems/DeadSystem.hpp"
#include "systems/PlayerSystem.hpp"
#include "systems/BulletSystem.hpp"
#include "systems/AudioSystem.hpp"
#include "systems/BulletSystem.hpp"
#include "systems/EnemyGarbageCollectorSystem.hpp"
#include "systems/EnemySpawnerSystem.hpp"
#include "systems/PlayerSystem.hpp"

#include "factoryDataDriven/factoryDataDriven.hpp"
#include "Protocol/EntityPayload.hpp"
#include "Protocol/SnapshotPayload.hpp"

#include <type_traits>
#include <variant>

namespace rtype::client {
Game::Game()
    : _window({1280.f, 720.f}, "R-Type"), _adapter(_window),
      _sceneManager(_window, std::in_place_type<MenuScene>) {}
void Game::onLoad(rtk::EngineContext &context) {
    registerComponents(context);
    createBindings();

    registerBuilders(context);
    loadEntityBluePrint(context);

    _networkClient.connect("127.0.0.1", "4242");

    uint32_t shootSoundId = _adapterAudioManager.addSound("assets/shoot.wav");

    subscribeEvents(context, shootSoundId);
    addSystems(context);

    _sceneManager.start(context);
}

void Game::loadEntityBluePrint(rtk::EngineContext &context)
{
    context._entityFactory.loadBlueprintFromPath("player", "assets/descriptionEntity/player.json");
    context._entityFactory.loadBlueprintFromPath("gate", "assets/descriptionEntity/gate.json");
    context._entityFactory.loadBlueprintFromPath("ramp", "assets/descriptionEntity/ramp.json");
    context._entityFactory.loadBlueprintFromPath("reactor", "assets/descriptionEntity/reactor.json");
    context._entityFactory.loadBlueprintFromPath("high_ground", "assets/descriptionEntity/high_ground.json");
    context._entityFactory.loadBlueprintFromPath("low_ground", "assets/descriptionEntity/low_ground.json");
    context._entityFactory.loadBlueprintFromPath("medium_ground", "assets/descriptionEntity/medium_ground.json");
    context._entityFactory.loadBlueprintFromPath("bullet", "assets/descriptionEntity/bullet.json");
    context._entityFactory.loadBlueprintFromPath("enemyBidos", "assets/descriptionEntity/enemyBidos.json");

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
    context._entityFactory.registerBuilder("animation", component::animationFromJson);
    context._entityFactory.registerBuilder("Animation", component::animationFromJson);
    context._entityFactory.registerBuilder("dead", component::deadFromJson);
    context._entityFactory.registerBuilder("Dead", component::deadFromJson);
    context._entityFactory.registerBuilder("networkEntity", component::networkEntityFromJson);
    context._entityFactory.registerBuilder("NetworkEntity", component::networkEntityFromJson);
    context._entityFactory.registerBuilder("soundRequest", component::soundRequestFromJson);
    context._entityFactory.registerBuilder("SoundRequest", component::soundRequestFromJson);
    context._entityFactory.registerBuilder("bullet", component::bulletFromJson);
    context._entityFactory.registerBuilder("Bullet", component::bulletFromJson);
    context._entityFactory.registerBuilder("controllable", component::controllableFromJson);
    context._entityFactory.registerBuilder("Controllable", component::controllableFromJson);
    context._entityFactory.registerBuilder("enemy", component::enemyFromJson);
    context._entityFactory.registerBuilder("Enemy", component::enemyFromJson);
    context._entityFactory.registerBuilder("enemyEvent", component::enemyEventFromJson);
    context._entityFactory.registerBuilder("EnemyEvent", component::enemyEventFromJson);
    context._entityFactory.registerBuilder("enemyEscapedEvent", rtype::events::enemyEscapedEventFromJson);
    context._entityFactory.registerBuilder("EnemyEscapedEvent", rtype::events::enemyEscapedEventFromJson);
    context._entityFactory.registerBuilder("Animator", component::animatorFromJson);
    context._entityFactory.registerBuilder("animator", component::animatorFromJson);



    context._entityFactory.registerBuilder("sprite", [this](rtk::ecs::Registry& reg, std::size_t entity, const nlohmann::json& data) {
        rtk::ecs::concepts::SpriteData sprite{};
        const std::string texPath = data.value("texture", "default.png");

        sprite.textureId = _window.loadTexture(texPath.data()).getHandle();
        sprite.size = {data["size"].value("w", 0.f), data["size"].value("h", 0.f)};
        sprite.origin = {data["origin"].value("x", 0.f), data["origin"].value("y", 0.f)};
        sprite.textureRect = {
            data["textureRect"].value<uint16_t>("x", 0),
            data["textureRect"].value<uint16_t>("y", 0),
            data["textureRect"].value<uint16_t>("w", 0),
            data["textureRect"].value<uint16_t>("h", 0)
        };
        sprite.color = {255, 255, 255, 255};
        sprite.layer = data.value("layer", -1);
        sprite.flags = data.value("flags", 0);

        reg.get_components<rtk::ecs::concepts::SpriteData>().insert_at(entity, sprite);
    });
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
}

void Game::createBindings() {
    _inputMapping.bindKey(0, rtk::ecs::concepts::Key::Up);
    _inputMapping.bindGamepad(0, rtk::ecs::concepts::GamepadButton::DpadUp);
    _inputMapping.bindGamepadAxis(0, rtk::ecs::concepts::GamepadAxis::LeftY, -0.30f);

    _inputMapping.bindKey(1, rtk::ecs::concepts::Key::Down);
    _inputMapping.bindGamepad(1, rtk::ecs::concepts::GamepadButton::DpadDown);
    _inputMapping.bindGamepadAxis(1, rtk::ecs::concepts::GamepadAxis::LeftY, 0.30f);

    _inputMapping.bindKey(2, rtk::ecs::concepts::Key::Left);
    _inputMapping.bindGamepad(2, rtk::ecs::concepts::GamepadButton::DpadLeft);
    _inputMapping.bindGamepadAxis(2, rtk::ecs::concepts::GamepadAxis::LeftX, -0.30f);

    _inputMapping.bindKey(3, rtk::ecs::concepts::Key::Right);
    _inputMapping.bindGamepad(3, rtk::ecs::concepts::GamepadButton::DpadRight);
    _inputMapping.bindGamepadAxis(3, rtk::ecs::concepts::GamepadAxis::LeftX, 0.30f);

    _inputMapping.bindKey(4, rtk::ecs::concepts::Key::Space);
    _inputMapping.bindGamepadAxis(4, rtk::ecs::concepts::GamepadAxis::R2, 0.30f);

    _inputMapping.bindKey(5, rtk::ecs::concepts::Key::Enter);
    _inputMapping.bindGamepad(5, rtk::ecs::concepts::GamepadButton::Cross);

    _inputMapping.bindKey(6, rtk::ecs::concepts::Key::F3);
    _inputMapping.bindGamepad(6, rtk::ecs::concepts::GamepadButton::Select);
}

void Game::subscribeEvents(rtk::EngineContext &context, uint32_t shootSoundId) {
    context.eventBus.subscribe<rtype::events::SendPacketEvent>(
        [this](const rtype::events::SendPacketEvent& event) {
            _networkClient.send(event.data);
        }
    );

    context.eventBus.subscribe<rtype::events::PlayerShotEvent>(
        [this, shootSoundId](const rtype::events::PlayerShotEvent&) {
            _adapterAudioManager.playSound(shootSoundId);
        }
    );

    context.eventBus.subscribe<rtype::events::EnemyEscapedEvent>(
        [this, shootSoundId](const rtype::events::EnemyEscapedEvent&) {
            _adapterAudioManager.playSound(shootSoundId);
        }
    );

    context.eventBus.subscribe<rtk::collision::Collision>(
        [&context](const rtk::collision::Collision& event) {
            auto &colliders = context.registry.get_components<component::AABBCollider>();
            auto *colA = colliders.get(event.firstEntity);
            auto *colB = colliders.get(event.secondEntity);

            if (!colA || !colB) return;

            std::uint32_t collisionPair = colA->layer | colB->layer;
            std::uint32_t bulletVsEnemy = Collider::CollisionLayer::PlayerProjectile | Collider::CollisionLayer::Enemy;

            if (collisionPair == bulletVsEnemy) {
                auto &deads = context.registry.get_components<component::Dead>();
                deads.insert_at(event.firstEntity, {});
                deads.insert_at(event.secondEntity, {});
            }
        }
    );
}

void Game::addSystems(rtk::EngineContext &context) {
    context.scheduler.add<rtk::systems::AnimationSystem>(rtk::ecs::Order::Render);
    context.scheduler.add<rtk::systems::DeathSystem>(rtk::ecs::Order::Update);
    //context.scheduler.add<rtk::systems::PlayerSystem>(rtk::ecs::Order::Update, std::ref(_inputState), std::ref(context.eventBus), std::ref(context._entityFactory));
    //context.scheduler.add<rtk::systems::BulletSystem>(rtk::ecs::Order::Update);
    //context.scheduler.add<rtk::systems::EnemySpawnerSystem>(rtk::ecs::Order::Update, std::ref(context._entityFactory));
    context.scheduler.add<rtk::systems::EnemyGCSystem>(rtk::ecs::Order::SndUpdate, context.eventBus);
    context.scheduler.add<rtk::systems::InputSystem<RtkWindowAdapter>>(rtk::ecs::Order::Input, std::ref(_adapter), _inputState, _inputMapping);
    //context.scheduler.add<rtk::systems::MovementSystem>(rtk::ecs::Order::Physics);
    context.scheduler.add<rtk::systems::RenderSystem<RtkWindowAdapter>>(rtk::ecs::Order::Render, std::ref(_adapter));
    context.scheduler.add<rtk::systems::DebugCollisionSystem>(rtk::ecs::Order::Physics, _collisionBuffer, _window, _inputState);
    context.scheduler.add<rtk::systems::ColliderSystem>(rtk::ecs::Order::Physics, _collisionBuffer, 64.f, context.eventBus);
    context.scheduler.add<rtk::systems::AudioSystem<RtkAudioAdapter>>(rtk::ecs::Order::Update, std::ref(_adapterAudioManager));
    context.scheduler.add<rtk::systems::AnimatorSystem>(rtk::ecs::Order::Update);
}

bool Game::shouldContinue() const { return !_inputState.closeRequested; }

void Game::onUpdate(rtk::EngineContext &context, float dt)
{
  NetworkEvent netEvent;

  while (_networkClient.pollEvent(netEvent)) {
    if (netEvent.type == NetworkEventType::Connected) {
        context.eventBus.publish(
            rtype::events::ConnectedToServerEvent{}
        );
    }
    else if (netEvent.type == NetworkEventType::Rejected) {
      context.eventBus.publish(
          rtype::events::ConnectionRejectedEvent{}
      );
    }
  }

  _sceneManager.update(context, _inputState, dt);

  if (_sceneManager.get<GameScene>() != nullptr) {
    handleReplication(context);
  }
}

void Game::onUnload(rtk::EngineContext &context) {
  for (const auto &[networkId, entity] : _networkEntities) {
    (void)networkId;
    context.registry.kill_entity(entity);
  }
  _networkEntities.clear();
  _sceneManager.stop(context);
  _networkClient.stop();

}

void Game::createReplicatedEntity(rtk::EngineContext &context,
    const protocol::CreateEntityPayload &payload) {
  if (_networkEntities.contains(payload.entityId))
    return;

  const auto entity = context._entityFactory.createNetwork(context.registry,
    payload.blueprintId, payload.entityId);
    
  auto *transform = context.registry.get_components<component::Transform>().get(entity);
  if (!transform) {
    context.registry.kill_entity(entity);
    return;
  }
  
  transform->position = {payload.x, payload.y};
  _networkEntities.emplace(payload.entityId, entity);
}

void Game::destroyReplicatedEntity(rtk::EngineContext &context,
  const protocol::DestroyEntityPayload &payload) {
  
  const auto it = _networkEntities.find(payload.entityId);

    if (it == _networkEntities.end())
        return;

    context.registry.kill_entity(it->second);
    _networkEntities.erase(it);
}

void Game::applySnapshot(rtk::EngineContext &context, const protocol::SnapshotPayload &payload) {
  const auto delta = static_cast<std::int32_t>(payload.serverTick - _lastServerTick);

  if (_lastServerTick != 0 && delta < 0)
    return;

  auto &transforms = context.registry.get_components<component::Transform>();

  for (const auto &state : payload.entities) {
    const auto it = _networkEntities.find(state.entityId);
    if (it == _networkEntities.end())
      continue;

    auto *transform = transforms.get(it->second);
    if (!transform)
      continue;
    transform->position = {state.x, state.y};
  }
    _lastServerTick = payload.serverTick;
}

void Game::handleReplication(rtk::EngineContext &context) {
  ReplicationEvent event;

  while(_networkClient.pollReplicationEvent(event)) {
    std::visit([&](const auto &payload){
      using T = std::decay_t<decltype(payload)>;
      if constexpr (std::is_same_v<T, protocol::CreateEntityPayload>) {
          createReplicatedEntity(context, payload); 
      } else if constexpr (std::is_same_v<T, protocol::DestroyEntityPayload>) {
          destroyReplicatedEntity(context, payload);
      } else if constexpr (std::is_same_v<T, protocol::SnapshotPayload>) {
        applySnapshot(context, payload);
      }
    },
    event.payload
  );}
}



} // namespace rtype::client
