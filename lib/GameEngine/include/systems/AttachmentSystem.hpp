#pragma once
#include "components/AttachedTo.hpp"
#include "components/Dead.hpp"
#include "components/Transform.hpp"
#include <ISystem.hpp>
#include <iostream>

namespace rtk::systems {
/**
 * @addtogroup api_engine
 * @{
 */

/**
 * @brief Moves every AttachedTo entity with its target, and kills it once
 * the target is gone.
 */
class AttachmentSystem final : public rtk::ecs::ISystem {
public:
  /**
   * @brief Does nothing.
   */
  void onStart(rtk::ecs::Registry &) override {}

  /**
   * @brief Places each attached entity at its target position plus offset.
   * @param reg Registry holding the entities.
   */
  void update(rtk::ecs::Registry &reg, float) override {
    auto view = reg.view<component::Transform, component::AttachedTo>();
    auto &transforms = reg.get_components<component::Transform>();
    auto &attachments = reg.get_components<component::AttachedTo>();

    for (auto entity : view) {
      auto *transform = transforms.get(entity);
      auto *attached = attachments.get(entity);
      if (!transform || !attached)
        continue;
      auto *targetTransform = transforms.get(attached->targetEntity);
      bool isAlive = false;
      auto targetView = reg.view<component::Transform>();
      for (auto e : targetView) {
        if (e == attached->targetEntity) {
          isAlive = true;
          break;
        }
      }
      if (targetTransform && isAlive) {
        transform->position.x = targetTransform->position.x + attached->offsetX;
        transform->position.y = targetTransform->position.y + attached->offsetY;
      } else {
        reg.remove_entity_from_all_pools(entity);
        reg.kill_entity(entity);
      }
    }
  }
  /**
   * @brief Does nothing.
   */
  void onStop(rtk::ecs::Registry &) override {}
};

/** @} */
} // namespace rtk::systems