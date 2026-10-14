#pragma once
#include <ISystem.hpp>

#include "Logger/Logger.hpp"
#include "inputAction/InputAction.hpp"

#include "input/action.hpp"

#include "components/Animator.hpp"
#include "components/Bullet.hpp"
#include "components/Controllable.hpp"
#include "components/NetworkEntity.hpp"
#include "components/Transform.hpp"
#include "components/Velocity.hpp"

#include "NetworkClient.hpp"
#include "factoryDataDriven/factoryDataDriven.hpp"

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
  PlayerSystem(rtk::InputState &inputState, busEvent &eventBus,
               rtk::factory::EntityFactory &entityFactory,
               rtype::client::NetworkClient &networkClient)
      : _inputState(inputState), _eventBus(eventBus),
        _entityFactory(entityFactory), _networkClient(networkClient) {}

  ~PlayerSystem() override = default;

  void onStart(rtk::ecs::Registry &) override {
    LOG_INFO("Load Player System");
  }

  void update(rtk::ecs::Registry &, float) override {
    sendInputsToServer();
    handleShootingAudio();
    _clientTick++;
  }

private:
  bool _isChargingShoot{false};

  void sendInputsToServer() {
    if (!_networkClient.isConnected())
      return;

    rtype::protocol::InputAction networkActions =
        rtype::protocol::InputAction::NoneAction;

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

  void handleShootingAudio() {
    bool isShootPressed =
        rtype::Input::hasAction(_inputState.actionMask, rtype::Action::Shoot);

    if (isShootPressed) {
      _isChargingShoot = true;
    } else if (_isChargingShoot) {
      _isChargingShoot = false;
      _eventBus.publish(rtype::events::PlayerShotEvent{});
    }
  }

  void onStop(rtk::ecs::Registry &) override {}
};
} // namespace rtk::systems