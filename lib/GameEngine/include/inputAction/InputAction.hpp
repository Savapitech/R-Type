#pragma once

#include <cstdint>

namespace rtk {

struct InputState {
  uint32_t actionMask{0};
  bool closeRequested = false;
  bool debug = false;
};

} // namespace rtk