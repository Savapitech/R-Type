#pragma once
#include "components/Animation.hpp"
#include "components/Animator.hpp"
#include <ISystem.hpp>

namespace rtk::systems {
class AnimatorSystem final : public rtk::ecs::ISystem {
public:
  AnimatorSystem() = default;
  ~AnimatorSystem() = default;

  void onStart(rtk::ecs::Registry &) override {}

  void update(rtk::ecs::Registry &reg, float dt) override {
    auto view = reg.view<component::animator, component::Animation>();

    auto &animators = reg.get_components<component::animator>();
    auto &animations = reg.get_components<component::Animation>();

    for (auto entity : view) {
      auto *anim = animations.get(entity);
      auto *animator = animators.get(entity);

      if (animator->currentState != animator->previousState) {
        const auto &currentClip =
            animator->clips[static_cast<std::size_t>(animator->currentState)];

        anim->startY = currentClip.startY;
        anim->totalFrames = currentClip.totalFrames;
        anim->loop = currentClip.loop;
        anim->currentFrame = 0;
        anim->playing = true;
        animator->previousState = animator->currentState;
      }
    }
  }

  void onStop(rtk::ecs::Registry &) override {}
};
} // namespace rtk::systems