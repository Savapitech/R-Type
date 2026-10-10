#pragma once

#include <cstddef>
#include <unordered_set>
#include <vector>
#include <algorithm>

#include <ISystem.hpp>

#include "collision/collisionBuffer.hpp"


#include "components/AABBCollider.hpp"
#include "components/Transform.hpp"

namespace rtk::systems
{
    class DebugCollisionSystem final : public rtk::ecs::ISystem
    {
        public:
            explicit DebugCollisionSystem(const rtk::collision::CollisionBuffer& collisionBuffer, rtk::RenderWindow & window, const rtk::InputState &inputState) : _collisionBuffer(collisionBuffer), _window(window), _inputState(inputState) {}
            ~DebugCollisionSystem() override = default;

           void onStart(rtk::ecs::Registry&) override
            {
                constexpr std::array<std::uint8_t, 4> whitePixel{255, 255, 255, 255};
                const std::span<const std::uint8_t> pixels{whitePixel};

                _debugTextureId = _window.loadTextureFromMemory(pixels, 1, 1).getHandle();
            }

            void update(rtk::ecs::Registry& registry, float) override
            {
                _collidingEntities.clear();

                for (const auto& collision : _collisionBuffer.collisions()) {
                    _collidingEntities.insert(collision.firstEntity);
                    _collidingEntities.insert(collision.secondEntity);
                }

                std::vector<DebugLine> lines;

                {
                    auto view = registry.view<component::Transform, component::AABBCollider>();
                    auto& transforms = registry.get_components<component::Transform>();
                    auto& colliders = registry.get_components<component::AABBCollider>();

                    constexpr float thickness = 2.f;

                    for (const auto entity : view) {
                        const auto* transform = transforms.get(entity);
                        const auto* collider = colliders.get(entity);

                        if (!transform || !collider)
                            continue;

                        const float firstX = transform->position.x + collider->offsetX * transform->scale.x;
                        const float secondX = firstX + collider->width * transform->scale.x;
                        const float firstY = transform->position.y + collider->offsetY * transform->scale.y;
                        const float secondY = firstY + collider->height * transform->scale.y;

                        const float left = std::min(firstX, secondX);
                        const float top = std::min(firstY, secondY);
                        const float right = std::max(firstX, secondX);
                        const float bottom = std::max(firstY, secondY);
                        const float width = right - left;
                        const float height = bottom - top;
                        const bool colliding = _collidingEntities.contains(entity);

                        lines.push_back({left, top, width, thickness, colliding});
                        lines.push_back({left, bottom - thickness, width, thickness, colliding});
                        lines.push_back({left, top, thickness, height, colliding});
                        lines.push_back({right - thickness, top, thickness, height, colliding});
                    }
                }

                resizeDebugEntities(registry, lines.size());

                for (std::size_t index = 0; index < lines.size(); ++index)
                    updateDebugLine(registry, _debugEntities[index], lines[index]);
            }

            void onStop(rtk::ecs::Registry& registry) override
            {
                for (const auto entity : _debugEntities)
                    registry.kill_entity(entity);

                registry.flush();

                _debugEntities.clear();
                _collidingEntities.clear();
            }

        private:
            void clearDebugEntities(rtk::ecs::Registry& registry)
            {
                for (const auto entity : _debugEntities)
                    registry.kill_entity(entity);

                _debugEntities.clear();
            }

            struct DebugLine
            {
                float x;
                float y;
                float width;
                float height;
                bool colliding;
            };

    void resizeDebugEntities(rtk::ecs::Registry& registry, std::size_t count)
    {
        while (_debugEntities.size() < count) {
            const auto entity = registry.spawn_entity();

            registry.get_components<component::Transform>().insert_at(entity, component::Transform{});
            registry.get_components<ecs::concepts::SpriteData>().insert_at(entity, ecs::concepts::SpriteData{});

            _debugEntities.push_back(entity);
        }

        while (_debugEntities.size() > count) {
            registry.kill_entity(_debugEntities.back());
            _debugEntities.pop_back();
        }
    }

    void updateDebugLine(rtk::ecs::Registry& registry, std::size_t entity, const DebugLine& line)
    {
        auto* transform = registry.get_components<component::Transform>().get(entity);
        auto* sprite = registry.get_components<ecs::concepts::SpriteData>().get(entity);

        if (!transform || !sprite)
            return;

        transform->position = {line.x, line.y};
        transform->rotation = 0.f;
        transform->scale = {1.f, 1.f};

        if (!_inputState.debug)
            sprite->size = {0, 0};
        else
            sprite->size = {line.width, line.height};
        sprite->origin = {0.f, 0.f};
        sprite->textureRect = {0, 0, 1, 1};
        sprite->color = line.colliding ? ecs::concepts::ColorRGBA8{255, 0, 0, 255} : ecs::concepts::ColorRGBA8{0, 255, 0, 255};
        sprite->textureId = _debugTextureId;
    }

        const rtk::collision::CollisionBuffer& _collisionBuffer;
        rtk::RenderWindow & _window;
        const rtk::InputState &_inputState;

        std::uint32_t _debugTextureId = 0;

        std::vector<std::size_t> _debugEntities;
        std::unordered_set<std::size_t> _collidingEntities;
    };
}