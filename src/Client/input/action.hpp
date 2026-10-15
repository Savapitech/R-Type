#pragma once

#include <cstdint>

namespace rtype {
/**
 * @addtogroup api_client
 * @{
 */

/**
 * @brief Client actions, one bit of InputState::actionMask each.
 */
enum class Action : uint32_t {
  NoneAction = 0,  ///< No action.
  Up = 1 << 0,     ///< Move up.
  Down = 1 << 1,   ///< Move down.
  Left = 1 << 2,   ///< Move left.
  Right = 1 << 3,  ///< Move right.
  Shoot = 1 << 4,  ///< Shoot.
  Confirm = 1 << 5 ///< Validate a menu.
};

/**
 * @brief Helpers to read client actions.
 */
namespace Input {
/**
 * @addtogroup api_client
 * @{
 */

/**
 * @brief Tells whether an action is active in a mask.
 * @param engineMask Action mask of the frame.
 * @param action Action to test.
 * @return True if the action bit is set.
 */
[[nodiscard]]
constexpr bool hasAction(uint32_t engineMask, Action action) {
  return (engineMask & static_cast<uint32_t>(action)) != 0;
}

/** @} */
} // namespace Input

/** @} */
} // namespace rtype
