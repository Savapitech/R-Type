#include <cstddef>
#include <cstdlib>
#include <optional>
#include <exception>
#include <iostream>
#include <vector>

#include "GameEngine.hpp"
#include "components/Transform.hpp"
#include "systems/RenderSystem.hpp"
#include "systems/InputSystem.hpp"

enum class Scene {
    Menu,
    Game
};

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

    context.scheduler.add<rtk::systems::RenderSystem>(rtk::ecs::Order::Render,_window);
    context.scheduler.add<rtk::systems::InputSystem>(rtk::ecs::Order::Input,_window, _inputState);

    _menuTextureId =
    _window.loadTexture("menu.png").getHandle();

_shipTextureId =
    _window.loadTexture("sprite.png").getHandle();

    switchScene(
        context.registry,
        Scene::Menu
    );
}

bool shouldContinue() const
{
    return !_inputState.closeRequested;
}


void onUpdate(
    rtk::EngineContext& context,
    float dt
) {
    if (
        _scene == Scene::Menu &&
        rtk::Input::hasAction(
            _inputState.actions,
            rtk::InputAction::Confirm
        )
    ) {
        _requestedScene = Scene::Game;
    }

    if (_requestedScene.has_value()) {
        switchScene(
            context.registry,
            *_requestedScene
        );

        _requestedScene.reset();
    }

    if (_scene != Scene::Game) {
        return;
    }

    if (!_playerEntity.has_value()) {
        return;
    }

    auto& transforms =
        context.registry.get_components<
            component::Transform
        >();

    auto* transform =
        transforms.get(*_playerEntity);

    if (!transform) {
        return;
    }

    constexpr float speed = 300.f;

    if (rtk::Input::hasAction(
        _inputState.actions,
        rtk::InputAction::Up
    )) {
        transform->position.y -= speed * dt;
    }

    if (rtk::Input::hasAction(
        _inputState.actions,
        rtk::InputAction::Down
    )) {
        transform->position.y += speed * dt;
    }

    if (rtk::Input::hasAction(
        _inputState.actions,
        rtk::InputAction::Left
    )) {
        transform->position.x -= speed * dt;
    }

    if (rtk::Input::hasAction(
        _inputState.actions,
        rtk::InputAction::Right
    )) {
        transform->position.x += speed * dt;
    }
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

        void switchScene(rtk::ecs::Registry& registry, Scene nextScene) {
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
        _playerEntity.reset();
    }

    void createMenuScene(
        rtk::ecs::Registry& registry
    );

    void createGameScene(rtk::ecs::Registry& registry);


    Scene _scene = Scene::Menu;
    std::optional<Scene> _requestedScene;

    std::vector<std::size_t> _sceneEntities;

    rtk::RenderWindow _window;
    rtk::Event _event;

    rtk::InputState _inputState;

    std::uint32_t _menuTextureId = 0;
    std::uint32_t _shipTextureId = 0;

    std::optional<std::size_t> _playerEntity;
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

        if (i == 0) {
            _playerEntity = entity;
        }

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