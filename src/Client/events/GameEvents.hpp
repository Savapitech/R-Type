#pragma once

#include <cstdint>

namespace rtype::events {
/**
 * @addtogroup api_client
 * @{
 */

/**
 * @brief Published when the local player releases the shoot button.
 */
struct PlayerShotEvent {
  int beamLevel{0}; ///< Beam charge stage (0: standard/powerup, 1: stage 1, 2:
                    ///< stage 2, 3: max).
};

/**
 * @brief Published when a quit request is triggered from the menu or game.
 */
struct QuitGameEvent {};

/**
 * @brief Published during beam charging to update HUD and visual effects.
 */
struct BeamChargeEvent {
  float chargeRatio{
      0.0f}; ///< Value from 0.0 to 1.0 representing charge progress.
  int chargeLevel{
      0}; ///< Charge stage (0: tap/charging 1, 1: stage 1, 2: stage 2, 3: max).
  bool isCharging{false}; ///< True while beam is charging.
};

/** @} */
} // namespace rtype::events
