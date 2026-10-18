#pragma once

#include <vector>

#include <ISystem.hpp>

#include "components/Transform.hpp"
#include "events/busEvents.hpp"

#include "components/Dead.hpp"
#include "components/Enemy.hpp"
#include "components/EnemyEvent.hpp"

namespace rtk::systems {
/**
 * @addtogroup api_gameplay
 * @{
 */

/**
 * @brief Removes enemies that left the screen on the left side.
 */
class EnemyGCSystem final : public rtk::ecs::ISystem {
public:
  /**
   * @brief Creates the system.
   * @param eventBus Bus on which EnemyEscapedEvent is published.
   */
  EnemyGCSystem(busEvent &eventBus) : _eventBus(eventBus) {};
  ~EnemyGCSystem() override = default;

  /**
   * @brief Does nothing.
   */
  void onStart(rtk::ecs::Registry &) override {}

  /**
   * @brief Marks enemies with x below -100 as dead and publishes an event.
   * @param reg Registry holding the enemies.
   */
  void update(rtk::ecs::Registry &reg, float) override {
    auto view = reg.view<component::Transform, component::Enemy>();
    auto &transforms = reg.get_components<component::Transform>();

    std::vector<std::size_t> entitiesToDestroy;

    for (auto entity : view) {
      auto *transform = transforms.get(entity);
      if (!transform)
        continue;

      if (transform->position.x < -100.0f) {
        entitiesToDestroy.push_back(entity);
      }
    }

    for (auto entity : entitiesToDestroy) {
      reg.get_components<component::Dead>().insert_at(entity, {});
      _eventBus.publish(rtype::events::EnemyEscapedEvent{entity});
    }
  }

  /**
   * @brief Does nothing.
   */
  void onStop(rtk::ecs::Registry &) override {}

private:
  busEvent &_eventBus;
};

/** @} */
} // namespace rtk::systems
