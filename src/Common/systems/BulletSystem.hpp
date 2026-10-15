#pragma once

#include "Logger/Logger.hpp"
#include "components/Bullet.hpp"
#include "components/Dead.hpp"
#include "components/Transform.hpp"
#include <ISystem.hpp>

namespace rtk::systems {
/**
 * @addtogroup api_gameplay
 * @{
 */

/**
 * @brief Marks bullets that went past the right edge of the world as dead.
 */
class BulletSystem final : public rtk::ecs::ISystem {
public:
  BulletSystem() = default;
  ~BulletSystem() override = default;

  /**
   * @brief Logs that the system is loaded.
   */
  void onStart(rtk::ecs::Registry &) override {
    LOG_INFO("Load Bullet System");
  }

  /**
   * @brief Adds Dead to every bullet whose x is greater than 1920.
   * @param reg Registry holding the bullets.
   */
  void update(rtk::ecs::Registry &reg, float) override {
    auto view = reg.view<component::Transform, component::Bullet>();
    auto &transforms = reg.get_components<component::Transform>();

    for (auto entity : view) {
      auto *transform = transforms.get(entity);
      if (!transform)
        continue;

      if (transform->position.x > 1920.0f) {
        reg.get_components<component::Dead>().insert_at(entity, {});
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
