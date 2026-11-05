#pragma once

#include "Logger/Logger.hpp"
#include "components/AABBCollider.hpp"
#include "components/Parallax.hpp"
#include "components/Transform.hpp"
#include "components/Velocity.hpp"
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
    bool stopScroll = false;
    auto viewCol = reg.view<component::Transform, component::AABBCollider>();
    auto &colliders = reg.get_components<component::AABBCollider>();
    auto &transforms = reg.get_components<component::Transform>();
    auto &parallaxes = reg.get_components<component::Parallax>();

    for (auto entity : viewCol) {
      auto *col = colliders.get(entity);
      auto *transform = transforms.get(entity);
      if (col && transform &&
          (col->layer & Collider::CollisionLayer::BossRoom)) {
        if (transform->position.x <= 0.0f) {
          transform->position.x = 0.0f;
          stopScroll = true;
          break;
        }
      }
    }
    if (stopScroll) {
      auto viewVel = reg.view<component::Velocity, component::AABBCollider>();
      auto &velocities = reg.get_components<component::Velocity>();
      for (auto entity : viewVel) {
        auto *col = colliders.get(entity);
        auto *vel = velocities.get(entity);
        if (col && vel &&
            ((col->layer & Collider::CollisionLayer::Environment) ||
             (col->layer & Collider::CollisionLayer::BossRoom))) {
          vel->x = 0.0f;
        }
      }
    }
    for (auto entity : view) {
      auto *transform = transforms.get(entity);
      auto *parallax = parallaxes.get(entity);

      if (!transform || !parallax)
        continue;
      if (stopScroll == true) {
        parallax->speedX = 0.0f;
      } else {
        transform->position.x -= parallax->speedX * dt;

        if (transform->position.x <= -parallax->loopWidth) {
          transform->position.x += parallax->loopWidth * 3.0f;
        }
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
