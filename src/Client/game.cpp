#include "game.hpp"

#include "components/Transform.hpp"
#include "sprite/spriteData.hpp"
#include "systems/InputSystem.hpp"
#include "systems/RenderSystem.hpp"

namespace rtype::client
{
    Game::Game() : _window({1280.f, 720.f}, "R-Type"), _sceneManager(_window) {}

    void Game::onLoad(rtk::EngineContext& context)
    {
        context.registry.register_component<component::Transform>();
        context.registry.register_component<rtk::SpriteData>();

        context.scheduler.add<rtk::systems::InputSystem>(rtk::ecs::Order::Input, _window, _inputState);
        context.scheduler.add<rtk::systems::RenderSystem>(rtk::ecs::Order::Render, _window);

        _sceneManager.start(context);
    }

    bool Game::shouldContinue() const
    {
        return !_inputState.closeRequested;
    }

    void Game::onUpdate(rtk::EngineContext& context, float dt)
    {
        _sceneManager.update(context, _inputState, dt);
    }

    void Game::onUnload(rtk::EngineContext& context)
    {
        _sceneManager.stop(context);
    }
}