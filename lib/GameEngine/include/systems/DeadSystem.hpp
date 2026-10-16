#pragma once

#include "components/Dead.hpp"
#include <ISystem.hpp>
#include <vector>

namespace rtk::systems {
/**
 * @addtogroup api_engine
 * @{
 */

/**
 * @brief Destroys every entity tagged with Dead.
 */
class DeathSystem final : public rtk::ecs::ISystem {
public:
  DeathSystem() = default;
  ~DeathSystem() override = default;

  /**
   * @brief Does nothing.
   */
  void onStart(rtk::ecs::Registry &) override {}

  /**
   * @brief Removes the components of dead entities and kills them.
   * @param reg Registry holding the entities.
   */
  void update(rtk::ecs::Registry &reg, float) override {
    auto view = reg.view<component::Dead>();

    std::vector<std::size_t> toDestroy;
    for (auto entity : view) {
      toDestroy.push_back(entity);
    }

    for (auto entity : toDestroy) {
      reg.remove_entity_from_all_pools(entity);
      reg.kill_entity(entity);
    }
  }

  /**
   * @brief Does nothing.
   */
  void onStop(rtk::ecs::Registry &) override {}
};

/** @} */
} // namespace rtk::systems
