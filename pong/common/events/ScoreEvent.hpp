#pragma once

#include <cstdint>
#include "../components/PongComponents.hpp"

namespace pong::events {

/**
 * @brief Event published on the engine event bus when a score changes.
 */
struct ScoreChangedEvent {
  pong::component::Paddle::Side scorer = pong::component::Paddle::Side::Left;
  std::uint8_t scoreP1 = 0;
  std::uint8_t scoreP2 = 0;
};

} // namespace pong::events
