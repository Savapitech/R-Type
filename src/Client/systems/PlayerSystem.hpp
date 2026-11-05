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
  std::uint32_t _clientTick{0};

  static constexpr float SQUARE_2_DIVIDE_BY_2 = 0.7071f;

public:
  /**
   * @brief Creates the system.
   * @param inputState Input state of the frame.
   * @param eventBus Bus on which PlayerShotEvent is published.
   * @param entityFactory Entity factory of the engine.
   * @param networkClient Client used to send Input messages.
   */
  PlayerSystem(rtk::InputState &inputState, busEvent &eventBus,
               rtk::factory::EntityFactory &entityFactory,
               rtype::client::NetworkClient &networkClient)
      : _inputState(inputState), _eventBus(eventBus),
        _entityFactory(entityFactory), _networkClient(networkClient) {}

  ~PlayerSystem() override = default;

  /**
   * @brief Logs that the system is loaded.
   */
  void onStart(rtk::ecs::Registry &) override {
    LOG_INFO("Load Player System");
  }

  /**
   * @brief Sends an Input message and publishes a shot on button release.
   */
  void update(rtk::ecs::Registry &reg, float dt) override {
    for (auto ent : reg.view<component::Controllable>()) {
      if (auto *ctrl =
              reg.get_components<component::Controllable>().get(ent)) {
        if (ctrl->weaponTimer > 0.0f) {
          ctrl->weaponTimer -= dt;
          if (ctrl->weaponTimer <= 0.0f) {
            ctrl->weaponTimer = 0.0f;
            ctrl->weaponType = 0;
          }
        }
      }
    }

    sendInputsToServer();
    handleBeamChargeAndShooting(reg, dt);
    _clientTick++;
  }

private:
  bool _isChargingShoot{false};
  float _chargeTimer{0.0f};
  std::optional<std::size_t> _chargeEffectEntity{std::nullopt};

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
        auto *trans =
            reg.get_components<component::Transform>().get(*_chargeEffectEntity);
        auto *anim =
            reg.get_components<component::Animation>().get(*_chargeEffectEntity);
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

      _eventBus.publish(rtype::events::PlayerShotEvent{.beamLevel = firedLevel});
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
