#pragma once

#include <functional>
#include <stdexcept>
#include <vector>

#include <ISystem.hpp>

#include "components/Transform.hpp"
#include "sprite/spriteData.hpp"
#include "graphical/renderWindow.hpp"

namespace rtk::systems
{
    class RenderSystem final : public rtk::ecs::ISystem
    {
        public:
            // Scheduler::add forwards its arguments as rvalues, so the window is
            // passed wrapped in a std::ref to avoid being moved
            RenderSystem(std::reference_wrapper<rtk::RenderWindow> window)
                : _window(window.get())
            {}

            ~RenderSystem() = default;

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
