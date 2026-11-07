#pragma once

#include <cstddef>
#include <cstdint>

namespace pong {

/**
 * @brief Client actions for Pong, one bit of InputState::actionMask each.
 */
enum class Action : std::uint32_t {
  NoneAction = 0,
  P1Up = 1 << 0,    ///< Player 1 move up (W / Z).
  P1Down = 1 << 1,  ///< Player 1 move down (S).
  P2Up = 1 << 2,    ///< Player 2 move up (Up arrow).
  P2Down = 1 << 3,  ///< Player 2 move down (Down arrow).
  Confirm = 1 << 4, ///< Start game / validate (Space / Enter).
  Back = 1 << 5,    ///< Return to menu / quit scene (Escape).
  ToggleAudio = 1 << 6 ///< Switch audio engine plugin (S / A).
};

namespace Input {

/**
 * @brief Tells whether an action is active in a mask.
 * @param engineMask Action mask of the frame.
 * @param action Action to test.
 * @return True if the action bit is set.
 */
[[nodiscard]] constexpr bool hasAction(std::uint32_t engineMask,
                                       Action action) noexcept {
  return (engineMask & static_cast<std::uint32_t>(action)) != 0;
}

/**
 * @brief Maps an Action enum value to its corresponding action index for InputMapping.
 * @param action Action to map.
 * @return 0-based action index.
 */
[[nodiscard]] constexpr std::size_t actionIndex(Action action) noexcept {
  switch (action) {
  case Action::P1Up:
    return 0;
  case Action::P1Down:
    return 1;
  case Action::P2Up:
    return 2;
  case Action::P2Down:
    return 3;
  case Action::Confirm:
    return 4;
  case Action::Back:
    return 5;
  case Action::ToggleAudio:
    return 6;
  default:
    return 0;
  }
}

} // namespace Input

} // namespace pong
