#pragma once

#include <cstdint>

#include "utils/vec2.hpp"

namespace component
{
    struct Transform
    {
        float rotation;
        rtk::vec2 position;
        rtk::vec2 scale;
    };
}