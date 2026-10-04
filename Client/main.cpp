#include <cstdlib>
#include <exception>
#include <iostream>

#include "GameEngine.hpp"
#include "components/Transform.hpp"
#include "systems/RenderSystem.hpp"

class RTypeClientGame
{
public:
    RTypeClientGame()
        : _window({1280.f, 720.f}, "R-Type")
    {
    }

    void onLoad(rtk::EngineContext& context)
    {
        context.registry.register_component<component::Transform>();
        context.registry.register_component<rtk::SpriteData>();

        context.scheduler.add<rtk::systems::RenderSystem>(rtk::ecs::Order::Render, _window);

        createScene(context.registry);
    }

    bool shouldContinue()
    {
        if (!_window.pollEvents(_event)) [[unlikely]]
            return false;
        return  !_event.isKeyPressed(rtk::Key::A);
    }

    void onUnload(rtk::EngineContext&)
    {
    }

private:
    void createScene(rtk::ecs::Registry& registry);

    rtk::RenderWindow _window;
    rtk::Event _event{};
};

void RTypeClientGame::createScene(rtk::ecs::Registry& registry) {
    const auto textureId = _window.loadTexture("sprite.png").getHandle();

    for (int i = 0; i < 3; ++i) {
        const auto entity = registry.spawn_entity();

        component::Transform transform{};

        transform.position = {150.f + static_cast<float>(i) * 250.f,250.f};

        transform.rotation = 0.f;
        transform.scale = {1.f, 1.f};

        rtk::SpriteData sprite{};

        sprite.size = {64.f, 32.f};
        sprite.origin = {0.f, 0.f};
        sprite.textureRect = {0, 0, 32, 16};
        sprite.color = {255, 255, 255, 255};
        sprite.textureId = textureId;

        registry.get_components<component::Transform>().insert_at(entity, transform);

        registry.get_components<rtk::SpriteData>().insert_at(entity, sprite);
    }
}

int main()
{
    try {
        RTypeClientGame game;
        rtk::GameEngine engine;

        return engine.run(game);
    } catch (const std::exception& error) {
        std::cerr << "CLIENT: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}