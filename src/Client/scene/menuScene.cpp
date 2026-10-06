#include "menuScene.hpp"

#include "components/Transform.hpp"
#include "sprite/spriteData.hpp"

namespace rtype::client
{
    MenuScene::MenuScene(rtk::RenderWindow& window) : _window(window) {}

    void MenuScene::onEnter(rtk::EngineContext& context)
    {
        _textureId = _window.loadTexture("menu.png").getHandle();

        const auto entity = context.registry.spawn_entity();

        component::Transform transform{};
        transform.position = {0.f, 0.f};
        transform.rotation = 0.f;
        transform.scale = {1.f, 1.f};

        rtk::SpriteData sprite{};
        sprite.size = {1280.f, 720.f};
        sprite.origin = {0.f, 0.f};
        sprite.textureRect = {0, 0, 1280, 720};
        sprite.color = {255, 255, 255, 255};
        sprite.textureId = _textureId;

        context.registry.get_components<component::Transform>().insert_at(entity, transform);
        context.registry.get_components<rtk::SpriteData>().insert_at(entity, sprite);

        _entities.push_back(entity);
    }

    std::optional<rtk::SceneType> MenuScene::onUpdate(rtk::EngineContext&, const rtk::InputState& inputState, float)
    {
        if (rtk::Input::hasAction(inputState.actions, rtk::InputAction::Confirm))
            return rtk::SceneType::Game;

        return std::nullopt;
    }

    void MenuScene::onExit(rtk::EngineContext& context)
    {
        for (const auto entity : _entities)
            context.registry.kill_entity(entity);

        context.registry.flush();
        _entities.clear();
    }
}