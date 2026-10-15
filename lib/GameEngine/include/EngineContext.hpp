#pragma once

#include <Registry.hpp>
#include <Scheduler.hpp>

#include "Game.hpp"
#include "events/busEvents.hpp"
#include "factoryDataDriven/factoryDataDriven.hpp"
#include "inputAction/InputAction.hpp"

namespace rtk {
/**
 * @addtogroup api_engine
 * @{
 */

/**
 * @brief Engine services handed to the game and its scenes.
 */
struct EngineContext {
  ecs::Registry &registry;   ///< Entities and components.
  ecs::Scheduler &scheduler; ///< Systems run every frame.
  busEvent &eventBus;        ///< Event bus shared by systems.
  rtk::factory::EntityFactory
      _entityFactory; ///< Blueprint based entity factory.
};

/** @} */
} // namespace rtk
