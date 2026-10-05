#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "sceneConcept.hpp"
#include "graphical/renderWindow.hpp"

namespace rtype::client
{
    class MenuScene
    {
        public:
            explicit MenuScene(rtk::RenderWindow& window);
            ~MenuScene() = default;

            void onEnter(rtk::EngineContext& context);
            std::optional<SceneType> onUpdate(rtk::EngineContext& context, const rtk::InputState& inputState, float dt);
            void onExit(rtk::EngineContext& context);

        private:
            rtk::RenderWindow& _window;

            std::vector<std::size_t> _entities;
            std::uint32_t _textureId = 0;
    };
}