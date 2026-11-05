#pragma once

#include <ISystem.hpp>
#include <algorithm>

#include "../components/PongComponents.hpp"
#include "components/Transform.hpp"
#include "components/Velocity.hpp"

namespace pong::systems {

/**
 * @brief Handles paddle screen boundary clamping and safety checks.
 */
class PongPaddleSystem final : public rtk::ecs::ISystem {
public:
  static constexpr float PADDLE_HEIGHT = 64.0f;
  static constexpr float TOP_BOUND = 8.0f;
  static constexpr float BOTTOM_BOUND = 712.0f - PADDLE_HEIGHT;

  PongPaddleSystem() = default;
  ~PongPaddleSystem() override = default;

  void onStart(rtk::ecs::Registry &) override {}

  /**
   * @brief Clamps paddle positions and zeros velocity if pressing against boundary.
   * @param reg Entity registry.
   * @param dt Delta time in seconds.
   */
  void update(rtk::ecs::Registry &reg, float dt) override {
    (void)dt;
    auto &transforms = reg.get_components<::component::Transform>();
    auto &velocities = reg.get_components<::component::Velocity>();
    auto &paddles = reg.get_components<pong::component::Paddle>();

    auto paddleView = reg.view<::component::Transform, ::component::Velocity,
                               pong::component::Paddle>();
    for (auto ent : paddleView) {
      auto *transform = transforms.get(ent);
      auto *velocity = velocities.get(ent);
      auto *paddle = paddles.get(ent);
      if (!transform || !velocity || !paddle)
        continue;

      transform->position.y =
          std::clamp(transform->position.y, TOP_BOUND, BOTTOM_BOUND);
      if (transform->position.y <= TOP_BOUND && velocity->y < 0.0f) {
        velocity->y = 0.0f;
      } else if (transform->position.y >= BOTTOM_BOUND && velocity->y > 0.0f) {
        velocity->y = 0.0f;
      }
    }
  }

  void onStop(rtk::ecs::Registry &) override {}
};

} // namespace pong::systems
