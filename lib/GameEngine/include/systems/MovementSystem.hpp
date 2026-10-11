#pragma once
#include "Logger/Logger.hpp"
#include "components/Transform.hpp"
#include "components/Velocity.hpp"
#include <ISystem.hpp>

namespace rtk::systems {
class MovementSystem final : public rtk::ecs::ISystem {
public:
  MovementSystem() = default;
  ~MovementSystem() override = default;

  void onStart(rtk::ecs::Registry &) override {
    LOG_INFO("Load Movement System");
  }

  void update(rtk::ecs::Registry &reg, float dt) override {
    auto view = reg.view<component::Transform, component::Velocity>();

    auto &transforms = reg.get_components<component::Transform>();
    auto &velocities = reg.get_components<component::Velocity>();

    for (auto entity : view) {
      auto *transform = transforms.get(entity);
      auto *velocity = velocities.get(entity);

      if (!transform || !velocity) {
        continue;
      }

      transform->position.x += velocity->x * dt;
      transform->position.y += velocity->y * dt;
    }
  }

  void onStop(rtk::ecs::Registry &) override {}
};
} // namespace rtk::systems