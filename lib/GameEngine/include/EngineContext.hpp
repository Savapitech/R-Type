#pragma once

#include <Registry.hpp>
#include <Scheduler.hpp>

#include "Game.hpp"
#include "events/busEvents.hpp"
#include "factoryDataDriven/factoryDataDriven.hpp"
#include "inputAction/InputAction.hpp"

namespace rtk {
struct EngineContext {
  ecs::Registry &registry;
  ecs::Scheduler &scheduler;
  busEvent &eventBus;
  rtk::factory::EntityFactory _entityFactory;
};
} // namespace rtk