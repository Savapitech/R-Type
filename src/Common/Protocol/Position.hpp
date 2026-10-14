#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>

#include "BigEndian.hpp"

namespace rtype::protocol {

inline constexpr float POSITION_SCALE = 4.0f;
inline constexpr float POSITION_OFFSET = 8192.0f;
inline constexpr float MIN_POSITION = -POSITION_OFFSET;
inline constexpr float MAX_POSITION =
    static_cast<float>(std::numeric_limits<std::uint16_t>::max()) /
        POSITION_SCALE -
    POSITION_OFFSET;

inline be16 encodePosition(float position) {
  if (!std::isfinite(position))
    position = 0.0f;
  if (position < MIN_POSITION)
    position = MIN_POSITION;
  if (position > MAX_POSITION)
    position = MAX_POSITION;

  return static_cast<std::uint16_t>(
      std::lround((position + POSITION_OFFSET) * POSITION_SCALE));
}

inline float decodePosition(be16 encoded) {
  return static_cast<float>(encoded.get()) / POSITION_SCALE - POSITION_OFFSET;
}

// 0 is 0 radians and 65536 would be 2*pi as the payload is 16 a 16 bit unsigned int
inline be16 encodeRotation(float radians) {
  constexpr float turn = 2.0f * std::numbers::pi_v<float>;

  if (!std::isfinite(radians))
    radians = 0.0f;

  float turns = std::fmod(radians, turn) / turn;
  if (turns < 0.0f)
    turns += 1.0f;

  return static_cast<std::uint16_t>(std::lround(turns * 65536.0f) & 0xFFFF);
}

inline float decodeRotation(be16 encoded) {
  return static_cast<float>(encoded.get()) / 65536.0f * 2.0f *
         std::numbers::pi_v<float>;
}

} // namespace rtype::protocol
