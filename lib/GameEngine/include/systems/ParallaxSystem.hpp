#pragma once

#include "Logger/Logger.hpp"
#include "components/Parallax.hpp"
#include "components/Transform.hpp"
#include <ISystem.hpp>

namespace rtk::systems {
/**
 * @addtogroup api_engine
 * @{
 */

/**
 * @brief Scrolls background layers and wraps them to loop endlessly.
 */
class ParallaxSystem final : public rtk::ecs::ISystem {
public:
  ParallaxSystem() = default;
  ~ParallaxSystem() override = default;

  /**
   * @brief Logs that the system is loaded.
   */
  void onStart(rtk::ecs::Registry &) override {
    LOG_INFO("Load Parallax System");
  }

  /**
   * @brief Moves each layer left and jumps it three tiles right once off
   * screen.
   * @param reg Registry holding the layers.
   * @param dt Elapsed time since the last frame, in seconds.
   */
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

  /**
   * @brief Does nothing.
   */
  void onStop(rtk::ecs::Registry &) override {}
};

/** @} */
} // namespace rtk::systems
