#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "graphical/renderWindow.hpp"
#include "sceneManager/sceneConcept.hpp"

#include "sceneType.hpp"

namespace rtype::client
{
    class GameScene
    {
        public:
            explicit GameScene(rtk::RenderWindow& window);
            ~GameScene() = default;

            static constexpr SceneType Type = SceneType::Game;

            void onEnter(rtk::EngineContext& context);
            std::optional<SceneType> onUpdate(rtk::EngineContext& context, const rtk::InputState& inputState, float dt);
            void onExit(rtk::EngineContext& context);

        private:
            void updatePlayer(rtk::EngineContext& context, const rtk::InputState& inputState, float dt);

            rtk::RenderWindow& _window;

            std::vector<std::size_t> _entities;
            std::optional<std::size_t> _playerEntity;

            std::uint32_t _textureId = 0;
    };
}