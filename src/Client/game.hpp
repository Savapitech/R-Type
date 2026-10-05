#pragma once

#include "GameEngine.hpp"
#include "graphical/renderWindow.hpp"
#include "inputAction/InputAction.hpp"
#include "collision/collisionBuffer.hpp"
#include "scene/sceneManager.hpp"


namespace rtype::client
{
    class Game
    {
        public:
            Game();
            ~Game() = default;

            void onLoad(rtk::EngineContext& context);
            bool shouldContinue() const;
            void onUpdate(rtk::EngineContext& context, float dt);
            void onUnload(rtk::EngineContext& context);

        private:
            rtk::RenderWindow _window;
            rtk::InputState _inputState{};
            rtk::collision::CollisionBuffer _collisionBuffer;

            SceneManager _sceneManager;
    };
}