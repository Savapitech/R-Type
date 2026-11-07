#pragma once

#include <ISystem.hpp>
#include <algorithm>
#include <optional>

#include "Logger/Logger.hpp"
#include "events/GameEvents.hpp"
#include "inputAction/InputAction.hpp"

#include "input/action.hpp"

#include "components/Animation.hpp"
#include "components/Animator.hpp"
#include "components/AttachedTo.hpp"
#include "components/Bullet.hpp"
#include "components/Controllable.hpp"
#include "components/NetworkEntity.hpp"
#include "components/Transform.hpp"
#include "components/Velocity.hpp"

#include "LocalPrediction.hpp"
#include "NetworkClient.hpp"
#include "factoryDataDriven/factoryDataDriven.hpp"

namespace rtk::systems {
/**
 * @addtogroup api_client
 * @{
 */

/**
 * @brief Sends the local player actions to the server every frame and
 * triggers the shot sound.
 */
class PlayerSystem final : public rtk::ecs::ISystem {
private:
  rtk::InputState &_inputState;
  busEvent &_eventBus;
  rtk::factory::EntityFactory &_entityFactory;
  rtype::client::NetworkClient &_networkClient;
  rtype::client::LocalPrediction &_prediction;

  static constexpr float SQUARE_2_DIVIDE_BY_2 = 0.7071f;
  /// Speed of a ship whose Controllable component is missing.
  static constexpr float DEFAULT_SHIP_SPEED = 300.0f;

public:
  /**
   * @brief Creates the system.
   * @param inputState Input state of the frame.
   * @param eventBus Bus on which PlayerShotEvent is published.
   * @param entityFactory Entity factory of the engine.
   * @param networkClient Client used to send Input messages.
   * @param prediction Prediction of the ship of the local player.
   */
  PlayerSystem(rtk::InputState &inputState, busEvent &eventBus,
               rtk::factory::EntityFactory &entityFactory,
               rtype::client::NetworkClient &networkClient,
               rtype::client::LocalPrediction &prediction)
      : _inputState(inputState), _eventBus(eventBus),
        _entityFactory(entityFactory), _networkClient(networkClient),
        _prediction(prediction) {}

  ~PlayerSystem() override = default;

  /**
   * @brief Logs that the system is loaded.
   */
  void onStart(rtk::ecs::Registry &) override {
    LOG_INFO("Load Player System");
  }

  /**
   * @brief Sends the held actions, moves the predicted ship at once and
   * publishes a shot on button release.
   */
  void update(rtk::ecs::Registry &reg, float dt) override {
    for (auto ent : reg.view<component::Controllable>()) {
      if (auto *ctrl = reg.get_components<component::Controllable>().get(ent)) {
        if (ctrl->weaponTimer > 0.0f) {
          ctrl->weaponTimer -= dt;
          if (ctrl->weaponTimer <= 0.0f) {
            ctrl->weaponTimer = 0.0f;
            ctrl->weaponType = 0;
          }
        }
      }
    }

    const auto actions = sendInputsToServer();
    moveLocalShip(reg, dt, actions);
    handleBeamChargeAndShooting(reg, dt);
  }

private:
  bool _isChargingShoot{false};
  float _chargeTimer{0.0f};
  std::optional<std::size_t> _chargeEffectEntity{std::nullopt};

  /**
   * Moves the ship of the local player without waiting for the server, with
   * the movement the server applies (LocalPrediction). Each Snapshot then
   * corrects it.
   */
  void moveLocalShip(rtk::ecs::Registry &reg,
                     float dt, rtype::protocol::InputAction actions) {
    const auto ownId = _prediction.ownNetworkId();
    if (ownId == 0)
      return;

    auto &networkEntities = reg.get_components<component::NetworkEntity>();
    auto &transforms = reg.get_components<component::Transform>();
    auto &controllables = reg.get_components<component::Controllable>();
    auto &animators = reg.get_components<component::animator>();
    for (auto ent : reg.view<component::NetworkEntity, component::Transform>()) {
      const auto *netEnt = networkEntities.get(ent);
      auto *transform = transforms.get(ent);
      if (!netEnt || !transform || netEnt->id != ownId)
        continue;

      const auto *ctrl = controllables.get(ent);
      _prediction.step(dt, actions, ctrl ? ctrl->speed : DEFAULT_SHIP_SPEED,
                       _networkClient.inputTick());
      if (!_prediction.isActive())
        return;
      transform->position.x = _prediction.x();
      transform->position.y = _prediction.y();

      if (auto *anim = animators.get(ent)) {
        using rtype::protocol::InputAction;
        if (rtype::protocol::hasAction(actions, InputAction::Up))
          anim->currentState = component::animState::UP;
        else if (rtype::protocol::hasAction(actions, InputAction::Down))
          anim->currentState = component::animState::DOWN;
        else
          anim->currentState = component::animState::IDLE;
      }
      return;
    }
  }

  rtype::protocol::InputAction sendInputsToServer() {
    if (!_networkClient.isConnected())
      return rtype::protocol::InputAction::NoneAction;

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

    _networkClient.sendInput(_networkClient.inputTick(), networkActions);
    return networkActions;
  }

  void handleBeamChargeAndShooting(rtk::ecs::Registry &reg, float dt) {
    const bool isShootPressed =
        rtype::Input::hasAction(_inputState.actionMask, rtype::Action::Shoot);

    if (isShootPressed) {
      if (!_isChargingShoot) {
        _isChargingShoot = true;
        _chargeTimer = 0.0f;

        for (auto ent : reg.view<component::Controllable>()) {
          try {
            auto effectEnt =
                _entityFactory.createFromBlueprint(reg, "beam_charge_effect");
            auto *attached =
                reg.get_components<component::AttachedTo>().get(effectEnt);
            if (attached) {
              attached->targetEntity = ent;
            }
            _chargeEffectEntity = effectEnt;
          } catch (const std::exception &e) {
            LOG_ERROR(e.what());
          }
          break;
        }
      } else {
        _chargeTimer += dt;
      }

      int level = 0;
      if (_chargeTimer >= 2.2f) {
        level = 3;
      } else if (_chargeTimer >= 1.4f) {
        level = 2;
      } else if (_chargeTimer >= 0.6f) {
        level = 1;
      }

      const float ratio = std::clamp(_chargeTimer / 2.2f, 0.0f, 1.0f);

      if (_chargeEffectEntity.has_value()) {
        auto *trans = reg.get_components<component::Transform>().get(
            *_chargeEffectEntity);
        auto *anim = reg.get_components<component::Animation>().get(
            *_chargeEffectEntity);
        if (trans) {
          float scale = 1.0f;
          if (level == 2) {
            scale = 1.35f;
          } else if (level == 3) {
            scale = 1.75f;
          }
          trans->scale = {scale, scale};
        }
        if (anim) {
          if (level == 3) {
            anim->frameDuration = 0.025f;
          } else if (level == 2) {
            anim->frameDuration = 0.038f;
          } else {
            anim->frameDuration = 0.05f;
          }
        }
      }

      _eventBus.publish(rtype::events::BeamChargeEvent{
          .chargeRatio = ratio,
          .chargeLevel = level,
          .isCharging = true,
      });
    } else if (_isChargingShoot) {
      int firedLevel = 0;
      if (_chargeTimer >= 2.2f) {
        firedLevel = 3;
      } else if (_chargeTimer >= 1.4f) {
        firedLevel = 2;
      } else if (_chargeTimer >= 0.6f) {
        firedLevel = 1;
      }

      _isChargingShoot = false;
      _chargeTimer = 0.0f;

      if (_chargeEffectEntity.has_value()) {
        reg.remove_entity_from_all_pools(*_chargeEffectEntity);
        reg.kill_entity(*_chargeEffectEntity);
        _chargeEffectEntity.reset();
      }

      _eventBus.publish(
          rtype::events::PlayerShotEvent{.beamLevel = firedLevel});
      _eventBus.publish(rtype::events::BeamChargeEvent{
          .chargeRatio = 0.0f,
          .chargeLevel = 0,
          .isCharging = false,
      });
    }
  }

  void onStop(rtk::ecs::Registry &reg) override {
    if (_chargeEffectEntity.has_value()) {
      reg.remove_entity_from_all_pools(*_chargeEffectEntity);
      reg.kill_entity(*_chargeEffectEntity);
      _chargeEffectEntity.reset();
    }
  }
};

/** @} */
} // namespace rtk::systems
