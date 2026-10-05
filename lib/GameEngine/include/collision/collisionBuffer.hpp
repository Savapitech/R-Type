#pragma once

#include <vector>

#include "components/AABBCollider.hpp"

namespace rtk::collision
{
    struct Collision
    {
        std::size_t firstEntity;
        std::size_t secondEntity;
    };

    class CollisionBuffer
    {
        public:
            void clear() { _collisions.clear(); }
            void push(const Collision& collision) { _collisions.push_back(collision); }
            const std::vector<Collision>& collisions() const { return _collisions; }

        private:
            std::vector<Collision> _collisions;
    };
}
