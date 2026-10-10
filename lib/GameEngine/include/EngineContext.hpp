#pragma once

#include <Registry.hpp>
#include <Scheduler.hpp>

#include "Game.hpp"
#include "inputAction/InputAction.hpp"

namespace rtk {
struct EngineContext {
  ecs::Registry &registry;
  ecs::Scheduler &scheduler;
};
} // namespace rtk