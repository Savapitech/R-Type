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

#include "factoryDataDriven/factoryDataDriven.hpp"

namespace rtk::systems {
class PlayerSystem final : public rtk::ecs::ISystem {
private:
  rtk::InputState &_inputState;
  busEvent &_eventBus;
  rtk::factory::EntityFactory &_entityFactory;
  static constexpr float SQUARE_2_DIVIDE_BY_2 = 0.7071f;

public:
  PlayerSystem(rtk::InputState &inputState, busEvent &eventBus, rtk::factory::EntityFactory &entityFactory)
      : _inputState(inputState), _eventBus(eventBus), _entityFactory(entityFactory) {}

  ~PlayerSystem() override = default;

  void onStart(rtk::ecs::Registry &) override {
    LOG_INFO("Load Player System");
  }

  void update(rtk::ecs::Registry &reg, float) override {
    auto view = reg.view<component::Velocity, component::Controllable, component::Transform, component::animator>();

    auto &velocities = reg.get_components<component::Velocity>();
    auto &controllables = reg.get_components<component::Controllable>();
    auto &transforms = reg.get_components<component::Transform>();
    auto &animators = reg.get_components<component::animator>();

    std::vector<component::Transform> bulletsToSpawn;

    for (auto entity : view) {
      auto *velocity = velocities.get(entity);
      auto *controllable = controllables.get(entity);
      auto *playerTransform = transforms.get(entity);
      auto *animator = animators.get((entity));

      if (!velocity || !controllable || !playerTransform || !animator)
        continue;

      velocity->x = 0.0f;
      velocity->y = 0.0f;

      if (rtype::Input::hasAction(_inputState.actionMask, rtype::Action::Up)) {
        velocity->y = -controllable->speed;
      }
      if (rtype::Input::hasAction(_inputState.actionMask,
                                  rtype::Action::Down)) {
        velocity->y = controllable->speed;
      }
      if (rtype::Input::hasAction(_inputState.actionMask,
                                  rtype::Action::Left)) {
        velocity->x = -controllable->speed;
      }
      if (rtype::Input::hasAction(_inputState.actionMask,
                                  rtype::Action::Right)) {
        velocity->x = controllable->speed;
      }

      if (velocity->x != 0.0f && velocity->y != 0.0f) {
        velocity->x *= SQUARE_2_DIVIDE_BY_2;
        velocity->y *= SQUARE_2_DIVIDE_BY_2;
      }

      if (velocity->y < 0.0f) {
        animator->currentState = component::animState::UP;
      } else if (velocity->y > 0.0f) {
        animator->currentState = component::animState::DOWN;
      } else {
        animator->currentState = component::animState::IDLE;
      }

      bool isShootPressed =
          rtype::Input::hasAction(_inputState.actionMask, rtype::Action::Shoot);

      if (isShootPressed) {
        controllable->isChargingShoot = true;
      } else if (controllable->isChargingShoot) {
        controllable->isChargingShoot = false;

        component::Transform spawnTransform = {};
        spawnTransform.position = {playerTransform->position.x + 50.0f,
                                   playerTransform->position.y};
        spawnTransform.rotation = 0.f;
        spawnTransform.scale = {1.f, 1.f};

        bulletsToSpawn.push_back(spawnTransform);
        _eventBus.publish(rtype::events::PlayerShotEvent{});
      }
    }

    for (const auto& spawnTransform : bulletsToSpawn) {
        auto bulletEntity = _entityFactory.createFromBlueprint(reg, "bullet");

        auto* transform = reg.get_components<component::Transform>().get(bulletEntity);

        if (transform) {
            transform->position = spawnTransform.position;
        }
    }
    }

  void onStop(rtk::ecs::Registry &) override {}
};
} // namespace rtk::systems