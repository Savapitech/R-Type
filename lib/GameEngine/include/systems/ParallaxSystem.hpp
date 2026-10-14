#pragma once
#include "Logger/Logger.hpp"
#include "components/Parallax.hpp"
#include "components/Transform.hpp"
#include <ISystem.hpp>

namespace rtk::systems {
class ParallaxSystem final : public rtk::ecs::ISystem {
public:
  ParallaxSystem() = default;
  ~ParallaxSystem() override = default;

  void onStart(rtk::ecs::Registry &) override {
    LOG_INFO("Load Parallax System");
  }

  void update(rtk::ecs::Registry &reg, float dt) override {
    auto view = reg.view<component::Transform, component::Parallax>();

    auto &transforms = reg.get_components<component::Transform>();
    auto &parallaxes = reg.get_components<component::Parallax>();

    for (auto entity : view) {
      auto *transform = transforms.get(entity);
      auto *parallax = parallaxes.get(entity);

      if (!transform || !parallax)
        continue;

      transform->position.x -= parallax->speedX * dt;

      if (transform->position.x <= -parallax->loopWidth) {
        transform->position.x += parallax->loopWidth * 3.0f;
      }
    }
  }

  void onStop(rtk::ecs::Registry &) override {}
};
} // namespace rtk::systems