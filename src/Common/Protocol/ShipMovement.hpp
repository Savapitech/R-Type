#pragma once

#include "InputPayload.hpp"

namespace rtype::protocol {
/**
 * @addtogroup api_protocol
 * @{
 */

/// Factor applied to both axes when a ship moves diagonally (1 / sqrt(2)).
inline constexpr float DIAGONAL_FACTOR = 0.7071f;

/// Velocity of a ship, in pixels per second.
struct ShipVelocity {
  float x = 0.0f; ///< Horizontal velocity, positive to the right.
  float y = 0.0f; ///< Vertical velocity, positive downwards.
};

/**
 * @brief Velocity of a ship that holds some actions.
 *
 * It is the single definition of how a ship moves: the server uses it to move
 * the ship, and the client uses it to predict the movement of its own ship, so
 * both agree.
 *
 * @param actions Actions held by the player.
 * @param speed Speed of the ship in pixels per second.
 * @return The velocity of the ship.
 */
constexpr ShipVelocity shipVelocity(InputAction actions, float speed) {
  ShipVelocity velocity;
  if (hasAction(actions, InputAction::Up))
    velocity.y = -speed;
  if (hasAction(actions, InputAction::Down))
    velocity.y = speed;
  if (hasAction(actions, InputAction::Left))
    velocity.x = -speed;
  if (hasAction(actions, InputAction::Right))
    velocity.x = speed;
  if (velocity.x != 0.0f && velocity.y != 0.0f) {
    velocity.x *= DIAGONAL_FACTOR;
    velocity.y *= DIAGONAL_FACTOR;
  }
  return velocity;
}

/** @} */
} // namespace rtype::protocol
