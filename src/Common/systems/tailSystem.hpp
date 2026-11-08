#pragma once

#include "components/Dead.hpp"
#include "components/Transform.hpp"
#include "components/bossTail.hpp"
#include <ISystem.hpp>
#include <cmath>

namespace rtk::systems {
/**
 * @addtogroup api_gameplay
 * @{
 */

/**
 * @brief Places every tail segment along a sine wave behind its body.
 */
class TailSystem final : public rtk::ecs::ISystem {
public:
  TailSystem() = default;
  ~TailSystem() override = default;

  /**
   * @brief Does nothing.
   */
  void onStart(rtk::ecs::Registry &) override {}

  /**
   * @brief Chains each segment from the body tip with a moving wave.
   * @param reg Registry holding the entities.
   * @param dt Elapsed time since the last frame, in seconds.
   */
  void update(rtk::ecs::Registry &reg, float dt) override {
    auto view = reg.view<component::Transform, component::tail>();
    auto &transforms = reg.get_components<component::Transform>();
    auto &tails = reg.get_components<component::tail>();
    auto &deads = reg.get_components<component::Dead>();

    for (auto entity : view) {
      if (deads.get(entity) != nullptr)
        continue;

      const auto *body = transforms.get(entity);
      auto *tail = tails.get(entity);
      if (!body || !tail)
        continue;

      tail->time += dt;
      float x = body->position.x + tail->tipX * body->scale.x;
      float y = body->position.y + tail->tipY * body->scale.y;
      for (std::size_t i = 0; i < tail->_segments.size(); i++) {
        const std::size_t segment = tail->_segments[i];
        if (deads.get(segment) == nullptr) {
          auto *segTransform = transforms.get(segment);
          if (segTransform) {
            segTransform->position.x = x;
            segTransform->position.y = y;
          }
        }
        const float angle =
            tail->baseAngle +
            tail->swing * std::sin(tail->time * tail->speed -
                                   static_cast<float>(i) * tail->phase);
        x += std::cos(angle) * tail->spacing;
        y += std::sin(angle) * tail->spacing;
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