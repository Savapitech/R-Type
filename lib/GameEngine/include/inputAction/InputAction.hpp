#pragma once

#include <cstdint>

namespace rtk {
enum class InputAction : std::uint8_t {
  NoneAction = 0,
  Up = 1 << 0,
  Down = 1 << 1,
  Left = 1 << 2,
  Right = 1 << 3,
  Shoot = 1 << 4,
  Confirm = 1 << 5
};

constexpr InputAction operator|(InputAction left, InputAction right) {
  return static_cast<InputAction>(static_cast<std::uint8_t>(left) |
                                  static_cast<std::uint8_t>(right));
}

constexpr InputAction &operator|=(InputAction &left, InputAction right) {
  left = left | right;
  return left;
}

struct InputState {
  uint32_t actionMask{0};
  bool closeRequested = false;
  bool debug = false;
};
} // namespace rtk