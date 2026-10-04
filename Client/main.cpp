enum class Scene {
    Menu,
    Game
};

#include <cstddef>
#include <cstdlib>
#include <optional>
#include <exception>
#include <iostream>
#include <vector>

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
    context.registry.register_component<
        component::Transform
    >();

    context.registry.register_component<
        rtk::SpriteData
    >();

    context.scheduler.add<
        rtk::systems::RenderSystem
    >(
        rtk::ecs::Order::Render,
        _window
    );

    _menuTextureId =
    _window.loadTexture("menu.png").getHandle();

_shipTextureId =
    _window.loadTexture("sprite.png").getHandle();

    switchScene(
        context.registry,
        Scene::Menu
    );
}

    bool shouldContinue()
{
    if (!_window.pollEvents(_event)) {
        return false;
    }

    if (
        _scene == Scene::Menu &&
        _event.isKeyPressed(rtk::Key::Enter)
    ) {
        _requestedScene = Scene::Game;
    }

    return true;
}

void onUpdate(
    rtk::EngineContext& context,
    float
) {
    if (!_requestedScene.has_value()) {
        return;
    }

    switchScene(
        context.registry,
        *_requestedScene
    );

    _requestedScene.reset();
}

    void onUnload(rtk::EngineContext&e)
    {
        clearScene(e.registry);
    }

private:
    enum class Scene {
        Menu,
        Game
    };

    void switchScene(
    rtk::ecs::Registry& registry,
    Scene nextScene
) {
    clearScene(registry);

    _scene = nextScene;

    switch (_scene) {
        case Scene::Menu:
            createMenuScene(registry);
            break;

        case Scene::Game:
            createGameScene(registry);
            break;
    }
}
void clearScene(rtk::ecs::Registry& registry)
{
    for (const auto entity : _sceneEntities) {
        registry.kill_entity(entity);
    }

    registry.flush();
    _sceneEntities.clear();
}

void createMenuScene(
    rtk::ecs::Registry& registry
);

void createGameScene(
    rtk::ecs::Registry& registry
);


    Scene _scene = Scene::Menu;
    std::optional<Scene> _requestedScene;

    std::vector<std::size_t> _sceneEntities;

    rtk::RenderWindow _window;
    rtk::Event _event;

    std::uint32_t _menuTextureId = 0;
    std::uint32_t _shipTextureId = 0;
};

void RTypeClientGame::createMenuScene(
    rtk::ecs::Registry& registry
) {
    const auto entity = registry.spawn_entity();

    component::Transform transform{};
    transform.position = {0.f, 0.f};
    transform.rotation = 0.f;
    transform.scale = {1.f, 1.f};

    rtk::SpriteData sprite{};
    sprite.size = {1280.f, 720.f};
    sprite.origin = {0.f, 0.f};

    sprite.textureRect = {0, 0, 1280, 720};

    sprite.color = {255, 255, 255, 255};
    sprite.textureId = _menuTextureId;

    registry
        .get_components<component::Transform>()
        .insert_at(entity, transform);

    registry
        .get_components<rtk::SpriteData>()
        .insert_at(entity, sprite);

    _sceneEntities.push_back(entity);
}

void RTypeClientGame::createGameScene(
    rtk::ecs::Registry& registry
) {
    for (int i = 0; i < 3; ++i) {
        const auto entity = registry.spawn_entity();

        component::Transform transform{};

        transform.position = {
            150.f + static_cast<float>(i) * 250.f,
            250.f
        };

        transform.rotation = 0.f;
        transform.scale = {1.f, 1.f};

        rtk::SpriteData sprite{};
        sprite.size = {64.f, 32.f};
        sprite.origin = {0.f, 0.f};
        sprite.textureRect = {0, 0, 32, 16};
        sprite.color = {255, 255, 255, 255};
        sprite.textureId = _shipTextureId;

        registry
            .get_components<component::Transform>()
            .insert_at(entity, transform);

        registry
            .get_components<rtk::SpriteData>()
            .insert_at(entity, sprite);

        _sceneEntities.push_back(entity);
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