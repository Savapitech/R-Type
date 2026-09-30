#pragma once

#include "include/ISystem.hpp"
#include "components/Transform.hpp"
#include "sprite/spriteData.hpp"
#include "graphical/renderWindow.hpp"

#include <stdexcept>
#include <vector>

namespace rtk::systems
{
    class RenderSystem final : public rtk::ecs::ISystem
    {
        public:
            RenderSystem(rtk::RenderWindow& window)
                : _window(window)
            {}

            ~RenderSystem() override = default;

            void onStart(rtk::ecs::Registry&) override
            {
            }

            void update(rtk::ecs::Registry& reg, float) override
            {
                _spritesToDraw.clear();

                auto view = reg.view<component::Transform, rtk::SpriteData>();

                auto& transforms = reg.get_components<component::Transform>();
                auto& sprites = reg.get_components<rtk::SpriteData>();

                for (auto entity : view) {
                    const auto* transform = transforms.get(entity);
                    const auto* sourceSprite = sprites.get(entity);

                    if (!transform || !sourceSprite)
                        continue;

                    auto sprite = *sourceSprite;
                    sprite.position = transform->position;
                    sprite.rotation = transform->rotation;
                    sprite.scale = transform->scale;

                    _spritesToDraw.push_back(std::move(sprite));
                }

                if (!_window.beginFrame(rtk::RGB {}))
                    return;

                _window.draw(_spritesToDraw);
                _window.endFrame();
            }

            void onStop(rtk::ecs::Registry&) override
            {
            }

        private:
            rtk::RenderWindow& _window;
            std::vector<rtk::SpriteData> _spritesToDraw;
    };
}