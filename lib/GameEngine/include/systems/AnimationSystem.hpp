#pragma once
#include <ISystem.hpp>
#include "components/Animation.hpp"
#include "systems/RenderConcept.hpp"
#include "Logger/Logger.hpp"

namespace rtk::systems {
    class AnimationSystem final : public rtk::ecs::ISystem {
    public:
        AnimationSystem() = default;
        ~AnimationSystem() override = default;

        void onStart(rtk::ecs::Registry&) override {
            LOG_INFO("Load Animation System");
        }

        void update(rtk::ecs::Registry& reg, float dt) override {
            auto view = reg.view<component::Animation, ecs::concepts::SpriteData>();

            auto& animations = reg.get_components<component::Animation>();
            auto& sprites = reg.get_components<ecs::concepts::SpriteData>();

            for (auto entity : view) {
                auto* anim = animations.get(entity);
                auto* sprite = sprites.get(entity);

                if (!anim || !sprite || !anim->playing || anim->totalFrames <= 1) {
                    continue;
                }

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
                uint16_t currentColumn = (anim->columns > 0) ? (anim->currentFrame % anim->columns) : anim->currentFrame;
                uint16_t currentRow = (anim->columns > 0) ? (anim->currentFrame / anim->columns) : 0;

                sprite->textureRect.x = anim->startX + (currentColumn * (anim->frameWidth + anim->paddingX));
                sprite->textureRect.y = anim->startY + (currentRow * (anim->frameHeight + anim->paddingY));
                sprite->textureRect.width = anim->frameWidth;
                sprite->textureRect.height = anim->frameHeight;
            }
        }

        void onStop(rtk::ecs::Registry&) override {}
    };
}