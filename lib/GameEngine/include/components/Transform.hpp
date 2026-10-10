#pragma once

#include <cstdint>

#include "../utils/vec2.hpp"

namespace component
{
    struct Transform
    {
        float rotation;
        utils::vec2 position;
        utils::vec2 scale;
    };
}