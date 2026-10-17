#pragma once
#include "components/AttachedTo.hpp"
#include "components/Dead.hpp"
#include "components/Transform.hpp"
#include <ISystem.hpp>
#include <iostream>

namespace rtk::systems {
class AttachmentSystem final : public rtk::ecs::ISystem {
public:
  void onStart(rtk::ecs::Registry &) override {}
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
      if (targetTransform) {
        transform->position.x = targetTransform->position.x + attached->offsetX;
        transform->position.y = targetTransform->position.y + attached->offsetY;
      } else {
        reg.get_components<component::Dead>().insert_at(entity, {});
      }
    }
  }
  void onStop(rtk::ecs::Registry &) override {}
};
} // namespace rtk::systems