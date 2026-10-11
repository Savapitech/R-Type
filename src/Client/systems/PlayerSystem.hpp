#pragma once
#include <ISystem.hpp>

#include "Logger/Logger.hpp"
#include "inputAction/InputAction.hpp"

#include "input/action.hpp"

#include "components/Velocity.hpp"
#include "components/Controllable.hpp"

namespace rtk::systems {
class PlayerSystem final : public rtk::ecs::ISystem {
private:
  rtk::InputState &_inputState;
  static constexpr float SQUARE_2_DIVIDE_BY_2 = 0.7071f;

public:
  PlayerSystem(rtk::InputState &inputState) : _inputState(inputState) {}
  ~PlayerSystem() override = default;

  void onStart(rtk::ecs::Registry &) override {
    LOG_INFO("Load Player System");
  }

  void update(rtk::ecs::Registry &reg, float ) override {
    auto view = reg.view<component::Velocity, component::Controllable>();

    auto &velocities = reg.get_components<component::Velocity>();
    auto &controllables = reg.get_components<component::Controllable>();

    for (auto entity : view) {
      auto *velocity = velocities.get(entity);
      auto *controllable = controllables.get(entity);

      if (!velocity || !controllable) continue;

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
  }

  void onStop(rtk::ecs::Registry &) override {}
};
} // namespace rtk::systems