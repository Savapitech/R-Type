#pragma once

#include <ISystem.hpp>
#include <algorithm>

#include "action.hpp"
#include "components/PongComponents.hpp"
#include "components/Transform.hpp"
#include "components/Velocity.hpp"
#include "systems/RenderConcept.hpp"

namespace pong::systems {

/**
 * @brief Handles player 1 and player 2 paddle movement and boundary clamping.
 */
class PongPaddleSystem final : public rtk::ecs::ISystem {
public:
  static constexpr float PADDLE_HEIGHT = 64.0f;
  static constexpr float TOP_BOUND = 8.0f;
  static constexpr float BOTTOM_BOUND = 712.0f - PADDLE_HEIGHT;

  /**
   * @brief Constructs the paddle system.
   * @param inputState Reference to current frame input state.
   */
  explicit PongPaddleSystem(const rtk::InputState &inputState)
      : _inputState(inputState) {}

  ~PongPaddleSystem() override = default;

  void onStart(rtk::ecs::Registry &) override {}

  void setEnabled(bool enabled) noexcept { _enabled = enabled; }
  [[nodiscard]] bool isEnabled() const noexcept { return _enabled; }

  /**
   * @brief Updates paddle velocities and limits vertical bounds.
   * @param reg Entity registry.
   * @param dt Delta time in seconds.
   */
  void update(rtk::ecs::Registry &reg, float dt) override {
    if (!_enabled)
      return;
    (void)dt;
    auto &transforms = reg.get_components<::component::Transform>();
    auto &velocities = reg.get_components<::component::Velocity>();
    auto &paddles = reg.get_components<pong::component::Paddle>();

    const bool p1Up =
        pong::Input::hasAction(_inputState.actionMask, pong::Action::P1Up);
    const bool p1Down =
        pong::Input::hasAction(_inputState.actionMask, pong::Action::P1Down);
    const bool p2Up =
        pong::Input::hasAction(_inputState.actionMask, pong::Action::P2Up);
    const bool p2Down =
        pong::Input::hasAction(_inputState.actionMask, pong::Action::P2Down);

    auto paddleView = reg.view<::component::Transform, ::component::Velocity,
                               pong::component::Paddle>();
    for (auto ent : paddleView) {
      auto *transform = transforms.get(ent);
      auto *velocity = velocities.get(ent);
      auto *paddle = paddles.get(ent);
      if (!transform || !velocity || !paddle)
        continue;

      if (paddle->side == pong::component::Paddle::Side::Left) {
        if (p1Up && !p1Down) {
          velocity->y = -paddle->speed;
        } else if (p1Down && !p1Up) {
          velocity->y = paddle->speed;
        } else {
          velocity->y = 0.0f;
        }
      } else {
        if (p2Up && !p2Down) {
          velocity->y = -paddle->speed;
        } else if (p2Down && !p2Up) {
          velocity->y = paddle->speed;
        } else {
          velocity->y = 0.0f;
        }
      }

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

private:
  const rtk::InputState &_inputState;
  bool _enabled = true;
};

} // namespace pong::systems
