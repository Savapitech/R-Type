#pragma once

#include <cstdint>

#include "BigEndian.hpp"

namespace rtype::protocol {
/**
 * @addtogroup api_protocol
 * @{
 */

/**
 * @brief Bit flags of the actions held by a player.
 */
enum class InputAction : std::uint8_t {
  NoneAction = 0x00, ///< No action held.
  Up = 0x01,         ///< Move up.
  Down = 0x02,       ///< Move down.
  Left = 0x04,       ///< Move left.
  Right = 0x08,      ///< Move right.
  Shoot = 0x10       ///< Shoot button held.
};

/**
 * @brief Combines two action sets.
 * @param left First action set.
 * @param right Second action set.
 * @return The union of both sets.
 */
constexpr InputAction operator|(InputAction left, InputAction right) {
  return static_cast<InputAction>(static_cast<std::uint8_t>(left) |
                                  static_cast<std::uint8_t>(right));
}

/**
 * @brief Tells whether an action set contains an action.
 * @param actions Action set to test.
 * @param action Action to look for.
 * @return True if the action is set.
 */
constexpr bool hasAction(InputAction actions, InputAction action) {
  return static_cast<std::uint8_t>(actions) & static_cast<std::uint8_t>(action);
}

/**
 * @brief Payload of an Input message, 9 bytes, sent by a client every frame
 * with the actions currently held.
 *
 * | Offset | Size | Field           | Description                            |
 * | -----: | ---: | --------------- | -------------------------------------- |
 * | 0      | 4    | `inputSequence` | Increases by one for each Input sent.  |
 * | 4      | 4    | `clientTick`    | Client frame counter.                  |
 * | 8      | 1    | `actions`       | InputAction bit flags.                 |
 */
struct InputPayload {
  be32 inputSequence; ///< Increases for each Input sent.
  be32 clientTick;    ///< Client frame counter.
  InputAction actions = InputAction::NoneAction; ///< Actions currently held.
};

static_assert(sizeof(InputPayload) ==
                  sizeof(be32) + sizeof(be32) + sizeof(InputAction),
              "InputPayload must not contain padding");

/** @} */
} // namespace rtype::protocol
