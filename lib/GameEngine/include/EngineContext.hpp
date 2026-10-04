#pragma once

#include <Registry.hpp>
#include <Scheduler.hpp>
#include "Game.hpp"

namespace rtk
{
    struct EngineContext
    {
        ecs::Registry& registry;
        ecs::Scheduler& scheduler;
    };
}