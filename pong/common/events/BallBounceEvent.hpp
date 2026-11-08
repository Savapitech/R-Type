#pragma once

#include "utils/vec2.hpp"

namespace pong::events {

/**
 * @brief Event published when the ball hits a paddle or wall.
 */
struct BallBounceEvent {
  utils::vec2 position{0.0f, 0.0f}; ///< Coordinates where the bounce occurred.
};

} // namespace pong::events
