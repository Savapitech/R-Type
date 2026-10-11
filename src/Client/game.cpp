#include "game.hpp"

#include "components/Transform.hpp"
#include "systems/AnimationSystem.hpp"
#include "systems/ColliderSystem.hpp"
#include "systems/DebugCollisionSystem.hpp"
#include "systems/InputSystem.hpp"
#include "systems/RenderConcept.hpp"
#include "systems/RenderSystem.hpp"
#include "systems/MovementSystem.hpp"

#include "components/Controllable.hpp"
#include "systems/PlayerSystem.hpp"

namespace rtype::client {
Game::Game()
    : _window({1280.f, 720.f}, "R-Type"), _adapter(_window),
      _sceneManager(_window, std::in_place_type<MenuScene>) {}

void Game::onLoad(rtk::EngineContext &context) {
  context.registry.register_component<component::Transform>();
  context.registry.register_component<rtk::ecs::concepts::SpriteData>();
  context.registry.register_component<component::AABBCollider>();
  context.registry.register_component<component::Animation>();
  context.registry.register_component<component::Controllable>();
  context.registry.register_component<component::Velocity>();


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

  context.scheduler.add<rtk::systems::AnimationSystem>(rtk::ecs::Order::Update);
  context.scheduler.add<rtk::systems::PlayerSystem>(rtk::ecs::Order::Update, std::ref(_inputState));
  context.scheduler.add<rtk::systems::InputSystem<RtkWindowAdapter>>(
      rtk::ecs::Order::Input, std::ref(_adapter), _inputState, _inputMapping);
  context.scheduler.add<rtk::systems::MovementSystem>(rtk::ecs::Order::Physics);
  context.scheduler.add<rtk::systems::RenderSystem<RtkWindowAdapter>>(rtk::ecs::Order::Render, std::ref(_adapter));
  context.scheduler.add<rtk::systems::DebugCollisionSystem>(
      rtk::ecs::Order::Physics, _collisionBuffer, _window, _inputState);
  context.scheduler.add<rtk::systems::ColliderSystem>(rtk::ecs::Order::Physics,
                                                      _collisionBuffer, 64.f, context.eventBus);

  _sceneManager.start(context);
}

bool Game::shouldContinue() const { return !_inputState.closeRequested; }

void Game::onUpdate(rtk::EngineContext &context, float dt) {
  _sceneManager.update(context, _inputState, dt);
}

void Game::onUnload(rtk::EngineContext &context) {
  _sceneManager.stop(context);
}
} // namespace rtype::client