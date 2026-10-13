#pragma once

#include <cstdint>

#include "BigEndian.hpp"

namespace rtype::protocol {
enum class InputAction : std::uint8_t {
  NoneAction = 0x00,
  Up = 0x01,
  Down = 0x02,
  Left = 0x04,
  Right = 0x08,
  Shoot = 0x10
};

constexpr InputAction operator|(InputAction left, InputAction right) {
  return static_cast<InputAction>(static_cast<std::uint8_t>(left) |
                                  static_cast<std::uint8_t>(right));
}

constexpr bool hasAction(InputAction actions, InputAction action) {
  return static_cast<std::uint8_t>(actions) & static_cast<std::uint8_t>(action);
}

struct InputPayload {
  be32 inputSequence;
  be32 clientTick;
  InputAction actions = InputAction::NoneAction;
};

static_assert(sizeof(InputPayload) ==
                  sizeof(be32) + sizeof(be32) + sizeof(InputAction),
              "InputPayload must not contain padding");
} // namespace rtype::protocol
