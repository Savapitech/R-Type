#pragma once
#include "Logger/Logger.hpp"
#include "components/Transform.hpp"
#include "components/Bullet.hpp"
#include <ISystem.hpp>

namespace rtk::systems {
class BulletSystem final : public rtk::ecs::ISystem {
public:
  BulletSystem() = default;
  ~BulletSystem() override = default;

  void onStart(rtk::ecs::Registry &) override {
    LOG_INFO("Load Bullet System");
  }

  void update(rtk::ecs::Registry &reg, float) override {
    auto view = reg.view<component::Transform, component::Bullet>();
    auto &transforms = reg.get_components<component::Transform>();

    for (auto entity : view) {
      auto *transform = transforms.get(entity);
      if (!transform) continue;

      if (transform->position.x > 1920.0f) {
        reg.kill_entity(entity);
      }
    }
  }

  void onStop(rtk::ecs::Registry &) override {}
};
} // namespace rtk::systems