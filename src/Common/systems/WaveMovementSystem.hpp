#pragma once

#include "Logger/Logger.hpp"
#include "components/Velocity.hpp"
#include "components/WaveMovement.hpp"
#include <ISystem.hpp>
#include <cmath>

namespace rtk::systems {
/**
 * @addtogroup api_gameplay
 * @{
 */

/**
 * @brief Updates vertical velocity of wave moving entities sinusoidally.
 */
class WaveMovementSystem final : public rtk::ecs::ISystem {
public:
  WaveMovementSystem() = default;
  ~WaveMovementSystem() override = default;

  /**
   * @brief Logs that the system is loaded.
   */
  void onStart(rtk::ecs::Registry &) override {
    LOG_INFO("Load Wave Movement System");
  }

  /**
   * @brief Updates vertical velocity according to cosine of elapsed wave time.
   * @param reg Registry holding the wave moving entities.
   * @param dt Elapsed time since the last frame, in seconds.
   */
  void update(rtk::ecs::Registry &reg, float dt) override {
    auto view = reg.view<component::WaveMovement, component::Velocity>();
    auto &waves = reg.get_components<component::WaveMovement>();
    auto &velocities = reg.get_components<component::Velocity>();

    for (auto entity : view) {
      auto *wave = waves.get(entity);
      auto *velocity = velocities.get(entity);

      if (!wave || !velocity) {
        continue;
      }

      wave->time += dt;
      velocity->y = wave->amplitude * wave->frequency *
                    std::cos(wave->frequency * wave->time);
    }
  }

  /**
   * @brief Does nothing.
   */
  void onStop(rtk::ecs::Registry &) override {}
};

/** @} */
} // namespace rtk::systems
