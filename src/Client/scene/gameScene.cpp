#include <sprite/spriteData.hpp>

#include "gameScene.hpp"
#include "components/Transform.hpp"
#include "components/AABBCollider.hpp"

namespace rtype::client
{
    GameScene::GameScene(rtk::RenderWindow& window) : _window(window) {}

    void GameScene::onEnter(rtk::EngineContext& context)
    {
        _textureId = _window.loadTexture("sprite.png").getHandle();

        for (int i = 0; i < 3; ++i) {
            const auto entity = context.registry.spawn_entity();

            if (i == 0)
                _playerEntity = entity;

            component::Transform transform{};
            transform.position = {150.f + static_cast<float>(i) * 250.f, 250.f};
            transform.rotation = 0.f;
            transform.scale = {1.f, 1.f};

            rtk::SpriteData sprite{};
            sprite.size = {64.f, 32.f};
            sprite.origin = {0.f, 0.f};
            sprite.textureRect = {0, 0, 32, 16};
            sprite.color = {255, 255, 255, 255};
            sprite.textureId = _textureId;

            component::AABBCollider collider{};
            collider.offsetX = 0.f;
            collider.offsetY = 0.f;
            collider.width = 64.f;
            collider.height = 32.f;
            collider.layer = Collider::CollisionLayer::Player;
            collider.mask = Collider::CollisionLayer::Enemy | Collider::CollisionLayer::EnemyProjectile | Collider::CollisionLayer::Player;

            context.registry.get_components<component::Transform>().insert_at(entity, transform);
            context.registry.get_components<rtk::SpriteData>().insert_at(entity, sprite);
            context.registry.get_components<component::AABBCollider>().insert_at(entity, collider);

            _entities.push_back(entity);
        }
    }

    std::optional<SceneType> GameScene::onUpdate(rtk::EngineContext& context, const rtk::InputState& inputState, float dt)
    {
        updatePlayer(context, inputState, dt);
        return std::nullopt;
    }

    void GameScene::onExit(rtk::EngineContext& context)
    {
        for (const auto entity : _entities)
            context.registry.kill_entity(entity);

        context.registry.flush();

        _entities.clear();
        _playerEntity.reset();
    }

    void GameScene::updatePlayer(rtk::EngineContext& context, const rtk::InputState& inputState, float dt)
    {
        if (!_playerEntity.has_value())
            return;

        auto& transforms = context.registry.get_components<component::Transform>();
        auto* transform = transforms.get(*_playerEntity);

        if (!transform)
            return;

        constexpr float speed = 300.f;

        if (rtk::Input::hasAction(inputState.actions, rtk::InputAction::Up))
            transform->position.y -= speed * dt;

        if (rtk::Input::hasAction(inputState.actions, rtk::InputAction::Down))
            transform->position.y += speed * dt;

        if (rtk::Input::hasAction(inputState.actions, rtk::InputAction::Left))
            transform->position.x -= speed * dt;

        if (rtk::Input::hasAction(inputState.actions, rtk::InputAction::Right))
            transform->position.x += speed * dt;
    }
}