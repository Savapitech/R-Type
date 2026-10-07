#pragma once

#include <cstdint>

namespace component
{
    struct AABBCollider
    {
        float offsetX = 0.f;
        float offsetY = 0.f;
        float width = 0.f;
        float height = 0.f;

        std::uint32_t layer = 1;
        std::uint32_t mask = 0xFFFFFFFF;

        bool trigger = true;
    };
}

namespace  Collider
{
    enum CollisionLayer : std::uint32_t
    {
        Player = 1 << 0,
        Enemy = 1 << 1,
        PlayerProjectile = 1 << 2,
        EnemyProjectile = 1 << 3
    };
}
