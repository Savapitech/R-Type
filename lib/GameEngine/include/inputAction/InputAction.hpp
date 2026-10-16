#pragma once

#include <cstdint>

namespace rtk {
/**
 * @addtogroup api_engine
 * @{
 */

/**
 * @brief Player input sampled during the current frame.
 */
struct InputState {
  uint32_t actionMask{0};      ///< Bit n is set while action n is active.
  bool closeRequested = false; ///< True when the window must close.
  bool debug = false;          ///< True while the debug overlay is enabled.
};

/** @} */
} // namespace rtk
