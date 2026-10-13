#pragma once
#include <ISystem.hpp>

#include "Logger/Logger.hpp"
#include "inputAction/InputAction.hpp"

#include "input/action.hpp"

#include "components/Animator.hpp"
#include "components/Bullet.hpp"
#include "components/Controllable.hpp"
#include "components/Transform.hpp"
#include "components/Velocity.hpp"
#include "components/NetworkEntity.hpp"

#include "factoryDataDriven/factoryDataDriven.hpp"
#include "NetworkClient.hpp"

namespace rtk::systems {
class PlayerSystem final : public rtk::ecs::ISystem {
private:
  rtk::InputState &_inputState;
  busEvent &_eventBus;
  rtk::factory::EntityFactory &_entityFactory;
  rtype::client::NetworkClient &_networkClient;
  std::uint32_t _clientTick{0};

  static constexpr float SQUARE_2_DIVIDE_BY_2 = 0.7071f;

public:
  PlayerSystem(rtk::InputState &inputState, busEvent &eventBus, rtk::factory::EntityFactory &entityFactory, rtype::client::NetworkClient &networkClient)
      : _inputState(inputState), _eventBus(eventBus), _entityFactory(entityFactory), _networkClient(networkClient) {}

  ~PlayerSystem() override = default;

  void onStart(rtk::ecs::Registry &) override {
    LOG_INFO("Load Player System");
  }

  void update(rtk::ecs::Registry &reg, float) override {
    sendInputsToServer();
    processLocalLogic(reg);
    _clientTick++;
  }

private:
  void sendInputsToServer() {
    if (!_networkClient.isConnected()) return;

    rtype::protocol::InputAction networkActions = rtype::protocol::InputAction::NoneAction;

    if (rtype::Input::hasAction(_inputState.actionMask, rtype::Action::Up)) {
      networkActions = networkActions | rtype::protocol::InputAction::Up;
    }
    if (rtype::Input::hasAction(_inputState.actionMask, rtype::Action::Down)) {
      networkActions = networkActions | rtype::protocol::InputAction::Down;
    }
    if (rtype::Input::hasAction(_inputState.actionMask, rtype::Action::Left)) {
      networkActions = networkActions | rtype::protocol::InputAction::Left;
    }
    if (rtype::Input::hasAction(_inputState.actionMask, rtype::Action::Right)) {
      networkActions = networkActions | rtype::protocol::InputAction::Right;
    }
    if (rtype::Input::hasAction(_inputState.actionMask, rtype::Action::Shoot)) {
      networkActions = networkActions | rtype::protocol::InputAction::Shoot;
    }

    _networkClient.sendInput(_clientTick, networkActions);
  }
  void processLocalLogic(rtk::ecs::Registry &reg) {
    auto view = reg.view<component::Velocity, component::Controllable, component::Transform, component::animator, component::NetworkEntity>();

    auto &velocities = reg.get_components<component::Velocity>();
    auto &controllables = reg.get_components<component::Controllable>();
    auto &transforms = reg.get_components<component::Transform>();
    auto &animators = reg.get_components<component::animator>();
    auto &networkEntities = reg.get_components<component::NetworkEntity>();

    for (auto entity : view) {
      auto *velocity = velocities.get(entity);
      auto *controllable = controllables.get(entity);
      auto *playerTransform = transforms.get(entity);
      auto *animator = animators.get(entity);
      auto *networkEntity = networkEntities.get(entity);

      if (!velocity || !controllable || !playerTransform || !animator || !networkEntity)
        continue;

      if (networkEntity->id == _networkClient.getConnectionId()) {
          velocity->x = 0.0f;
          velocity->y = 0.0f;

          if (rtype::Input::hasAction(_inputState.actionMask, rtype::Action::Up)) {
            velocity->y = -controllable->speed;
          }
          if (rtype::Input::hasAction(_inputState.actionMask, rtype::Action::Down)) {
            velocity->y = controllable->speed;
          }
          if (rtype::Input::hasAction(_inputState.actionMask, rtype::Action::Left)) {
            velocity->x = -controllable->speed;
          }
          if (rtype::Input::hasAction(_inputState.actionMask, rtype::Action::Right)) {
            velocity->x = controllable->speed;
          }

          if (velocity->x != 0.0f && velocity->y != 0.0f) {
            velocity->x *= SQUARE_2_DIVIDE_BY_2;
            velocity->y *= SQUARE_2_DIVIDE_BY_2;
          }
      }

      if (velocity->y < 0.0f) {
        animator->currentState = component::animState::UP;
      } else if (velocity->y > 0.0f) {
        animator->currentState = component::animState::DOWN;
      } else {
        animator->currentState = component::animState::IDLE;
      }

      if (networkEntity->id == _networkClient.getConnectionId()) {
          bool isShootPressed = rtype::Input::hasAction(_inputState.actionMask, rtype::Action::Shoot);

          if (isShootPressed) {
            controllable->isChargingShoot = true;
          } else if (controllable->isChargingShoot) {
            controllable->isChargingShoot = false;
            _eventBus.publish(rtype::events::PlayerShotEvent{});
          }
      }
    }
  }

  void onStop(rtk::ecs::Registry &) override {}
};
} // namespace rtk::systems