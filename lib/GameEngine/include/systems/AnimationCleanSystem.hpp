#pragma once
#include "Logger/Logger.hpp"
#include "Registry.hpp"
#include "components/Animation.hpp"
#include "components/Dead.hpp"
#include "components/DestroyAnimEnd.hpp"
#include "systems/RenderConcept.hpp"
#include <ISystem.hpp>

namespace rtk::systems {
/**
 * @addtogroup api_engine
 * @{
 */

/**
 * @brief Clean the animation which are finished.
 */
class AnimationCleanSystem final : public rtk::ecs::ISystem {
public:
  AnimationCleanSystem() = default;
  ~AnimationCleanSystem() override = default;

  /**
   * @brief Logs that the system is loaded.
   */
  void onStart(rtk::ecs::Registry &) override {
    LOG_INFO("Load AnimationClean System");
  }

  /**
   * @brief Check all entities with animation component, if the playing is
   * finished destroy it.
   * @param reg Registry holding the entities.
   * @param dt Unused.
   */
  void update(rtk::ecs::Registry &reg, float dt) override {
    auto view = reg.view<component::DestroyAnimEnd, component::Animation>();
    auto &animations = reg.get_components<component::Animation>();
    auto &destroyAnim = reg.get_components<component::DestroyAnimEnd>();
    auto &deads = reg.get_components<component::Dead>();

    for (auto entity : view) {
      auto *anim = animations.get((entity));

      if (!anim) {
        continue;
      }

      if (anim->playing == false) {
        deads.insert_at(entity, {});
      }
    }
  }
  /**
   * @brief Does nothing.
   */
  void onStop(rtk::ecs::Registry &reg) override {}
};
/** @} */
} // namespace rtk::systems