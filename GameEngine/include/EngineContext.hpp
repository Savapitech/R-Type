#pragma once

#include "include/Registry.hpp"
#include "include/Scheduler.hpp"
#include "Game.hpp"

namespace rtk
{
    struct EngineContext
    {
        ecs::Registry& registry;
        ecs::Scheduler& scheduler;
    };
}