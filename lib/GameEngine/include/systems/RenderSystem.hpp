#pragma once

#include <functional>
#include <stdexcept>
#include <vector>

#include <ISystem.hpp>
#include <Logger/Logger.hpp>
#include "components/Transform.hpp"

#include "systems/RenderConcept.hpp"


namespace rtk::systems
{
    template <rtk::ecs::concepts::IsRenderWindow WindowType>
    class RenderSystem final : public rtk::ecs::ISystem
    {
        public:
            RenderSystem(std::reference_wrapper<WindowType> window)
                : _window(window.get())
            {}

            ~RenderSystem() = default;

            void onStart(rtk::ecs::Registry&) override
            {
                LOG_INFO("Load Render System");
            }

            void update(rtk::ecs::Registry& reg, float) override
            {
                _spritesToDraw.clear();

                auto view = reg.view<component::Transform, rtk::ecs::concepts::SpriteData>();

                auto& transforms = reg.get_components<component::Transform>();
                auto& sprites = reg.get_components<rtk::ecs::concepts::SpriteData>();

                for (auto entity : view) {
                    const auto* transform = transforms.get(entity);
                    const auto* sourceSprite = sprites.get(entity);

                    if (!transform || !sourceSprite) continue;

                    rtk::ecs::concepts::SpriteData sprite = *sourceSprite;

                    sprite.position = transform->position;
                    sprite.rotation = transform->rotation;
                    sprite.scale = transform->scale;

                    _spritesToDraw.push_back(std::move(sprite));
                }

                if (!_window.beginFrame(rtk::ecs::concepts::ColorRGBA8{0, 0, 0, 255}))
                    return;

                _window.draw(_spritesToDraw);
                _window.endFrame();
            }

            void onStop(rtk::ecs::Registry&) override
            {
            }

        private:
            WindowType& _window;
            std::vector<rtk::ecs::concepts::SpriteData> _spritesToDraw;
    };
}