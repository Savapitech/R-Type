#pragma once
#include "Logger/Logger.hpp"
#include "components/Animation.hpp"
#include "systems/RenderConcept.hpp"
#include <ISystem.hpp>

namespace rtk::systems {
class AnimationSystem final : public rtk::ecs::ISystem {
public:
  AnimationSystem() = default;
  ~AnimationSystem() override = default;

  void onStart(rtk::ecs::Registry &) override {
    LOG_INFO("Load Animation System");
  }

  void update(rtk::ecs::Registry &reg, float dt) override {
    auto view = reg.view<component::Animation, ecs::concepts::SpriteData>();

    auto &animations = reg.get_components<component::Animation>();
    auto &sprites = reg.get_components<ecs::concepts::SpriteData>();

    for (auto entity : view) {
      auto *anim = animations.get(entity);
      auto *sprite = sprites.get(entity);

      if (!anim || !sprite) {
        continue;
      }

      if (anim->playing && anim->totalFrames > 1 && anim->frameDuration > 0.0f) {
        anim->elapsedTime += dt;

        while (anim->elapsedTime >= anim->frameDuration) {
          anim->elapsedTime -= anim->frameDuration;
          anim->currentFrame++;

          if (anim->currentFrame >= anim->totalFrames) {
            if (anim->loop) {
              anim->currentFrame = 0;
            } else {
              anim->currentFrame = anim->totalFrames - 1;
              anim->playing = false;
              break;
            }
          }
        }
      }

      uint16_t frameIdx = anim->currentFrame;
      if (anim->reverse && anim->totalFrames > 1) {
        frameIdx = (anim->currentFrame < anim->totalFrames)
                       ? (anim->totalFrames - 1 - anim->currentFrame)
                       : 0;
      }

      uint16_t currentColumn = (anim->columns > 0)
                                   ? (frameIdx % anim->columns)
                                   : frameIdx;
      uint16_t currentRow =
          (anim->columns > 0) ? (frameIdx / anim->columns) : 0;

      sprite->textureRect.x =
          anim->startX + (currentColumn * (anim->frameWidth + anim->paddingX));
      sprite->textureRect.y =
          anim->startY + (currentRow * (anim->frameHeight + anim->paddingY));
      if (anim->frameWidth > 0) {
        sprite->textureRect.width = anim->frameWidth;
      }
      if (anim->frameHeight > 0) {
        sprite->textureRect.height = anim->frameHeight;
      }
    }
  }

  void onStop(rtk::ecs::Registry &) override {}
};
} // namespace rtk::systems