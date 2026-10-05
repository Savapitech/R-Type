#pragma once

#include <cstddef>
#include <unordered_map>
#include <vector>

#include <ISystem.hpp>

#include "collision/collisionBuffer.hpp"
#include "components/AABBCollider.hpp"
#include "components/Transform.hpp"

namespace rtk::systems
{
    class ColliderSystem final : public rtk::ecs::ISystem
        {
        private:
            struct Cell
            {
                int x;
                int y;

                bool operator==(const Cell&) const = default;
            };

            struct CellHash
            {
                std::size_t operator()(const Cell& cell) const{
                    const auto x = static_cast<std::size_t>(static_cast<std::uint32_t>(cell.x));
                    const auto y = static_cast<std::size_t>(static_cast<std::uint32_t>(cell.y));

                    return x ^ (y << 32);
                }
            };

            struct WorldAABB
            {
                float left;
                float top;
                float right;
                float bottom;
            };

            WorldAABB getWorldAABB(const component::Transform& transform, const component::AABBCollider& collider) const
            {
                const float firstX = transform.position.x + collider.offsetX * transform.scale.x;
                const float secondX = firstX + collider.width * transform.scale.x;
                const float firstY = transform.position.y + collider.offsetY * transform.scale.y;
                const float secondY = firstY + collider.height * transform.scale.y;

                return {
                    .left = std::min(firstX, secondX),
                    .top = std::min(firstY, secondY),
                    .right = std::max(firstX, secondX),
                    .bottom = std::max(firstY, secondY)
                };
            }
            bool intersects(const WorldAABB& first, const WorldAABB& second) const
            {
                return first.left < second.right && first.right > second.left && first.top < second.bottom && first.bottom > second.top;
            }

            rtk::collision::CollisionBuffer& _collisionBuffer;
            float _cellSize;

            std::unordered_map<Cell, std::vector<std::size_t>, CellHash> _grid;

        public:

            ColliderSystem(rtk::collision::CollisionBuffer& collisionBuffer, float cellSize) : _collisionBuffer(collisionBuffer), _cellSize(cellSize) {}

            void onStart(rtk::ecs::Registry&)
            {
                LOG_INFO("Load Collider System");
            }

            void update(rtk::ecs::Registry& registry, float)
            {
                _grid.clear();
                _collisionBuffer.clear();

                auto view = registry.view<component::Transform, component::AABBCollider>();
                auto& transforms = registry.get_components<component::Transform>();
                auto& colliders = registry.get_components<component::AABBCollider>();

                std::unordered_map<std::size_t, WorldAABB> worldAABBs;

                for (const auto entity : view) {
                    const auto* transform = transforms.get(entity);
                    const auto* collider = colliders.get(entity);

                    if (!transform || !collider || collider->width <= 0.f || collider->height <= 0.f)
                        continue;

                    const auto aabb = getWorldAABB(*transform, *collider);

                    worldAABBs.emplace(entity, aabb);

                    const int minCellX = static_cast<int>(std::floor(aabb.left / _cellSize));
                    const int maxCellX = static_cast<int>(std::floor(std::nextafter(aabb.right, aabb.left) / _cellSize));
                    const int minCellY = static_cast<int>(std::floor(aabb.top / _cellSize));
                    const int maxCellY = static_cast<int>(std::floor(std::nextafter(aabb.bottom, aabb.top) / _cellSize));

                    for (int y = minCellY; y <= maxCellY; ++y) {
                        for (int x = minCellX; x <= maxCellX; ++x)
                            _grid[{x, y}].push_back(entity);
                    }
                }

                std::set<std::pair<std::size_t, std::size_t>> testedPairs;

                for (const auto& [cell, entities] : _grid) {
                    static_cast<void>(cell);

                    for (std::size_t firstIndex = 0; firstIndex < entities.size(); ++firstIndex) {
                        for (std::size_t secondIndex = firstIndex + 1; secondIndex < entities.size(); ++secondIndex) {
                            const auto firstEntity = std::min(entities[firstIndex], entities[secondIndex]);
                            const auto secondEntity = std::max(entities[firstIndex], entities[secondIndex]);

                            if (!testedPairs.emplace(firstEntity, secondEntity).second)
                                continue;

                            const auto* firstCollider = colliders.get(firstEntity);
                            const auto* secondCollider = colliders.get(secondEntity);

                            if (!firstCollider || !secondCollider)
                                continue;

                            if ((firstCollider->mask & secondCollider->layer) == 0)
                                continue;

                            if ((secondCollider->mask & firstCollider->layer) == 0)
                                continue;

                            const auto firstAABB = worldAABBs.find(firstEntity);
                            const auto secondAABB = worldAABBs.find(secondEntity);

                            if (firstAABB == worldAABBs.end() || secondAABB == worldAABBs.end())
                                continue;

                            if (!intersects(firstAABB->second, secondAABB->second))
                                continue;

                            _collisionBuffer.push({firstEntity, secondEntity});
                        }
                    }
                }
            }

            void onStop(rtk::ecs::Registry&)
            {
                _grid.clear();
                _collisionBuffer.clear();
            }
    };
}