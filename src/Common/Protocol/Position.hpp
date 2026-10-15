#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>

#include "BigEndian.hpp"

namespace rtype::protocol {
/**
 * @addtogroup api_protocol
 * @{
 */

/// Number of encoded steps per pixel, giving a 0.25 pixel resolution.
inline constexpr float POSITION_SCALE = 4.0f;
/// Offset added before encoding so that negative positions fit.
inline constexpr float POSITION_OFFSET = 8192.0f;
/// Smallest position that can be encoded, in pixels.
inline constexpr float MIN_POSITION = -POSITION_OFFSET;
/// Largest position that can be encoded, in pixels.
inline constexpr float MAX_POSITION =
    static_cast<float>(std::numeric_limits<std::uint16_t>::max()) /
        POSITION_SCALE -
    POSITION_OFFSET;

/**
 * @brief Encodes a position as a 16-bit fixed point value.
 * @param position Position in pixels, clamped to the encodable range.
 * @return `round((position + 8192) * 4)`, or the encoding of 0 if not finite.
 */
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

/**
 * @brief Decodes a position encoded by encodePosition().
 * @param encoded Encoded value.
 * @return The position in pixels.
 */
inline float decodePosition(be16 encoded) {
  return static_cast<float>(encoded.get()) / POSITION_SCALE - POSITION_OFFSET;
}

/**
 * @brief Encodes an angle as a fraction of a full turn on 16 bits.
 * @param radians Angle in radians, any value.
 * @return 0 for 0 radians, 32768 for pi, wrapping at 65536.
 */
inline be16 encodeRotation(float radians) {
  constexpr float turn = 2.0f * std::numbers::pi_v<float>;

  if (!std::isfinite(radians))
    radians = 0.0f;

  float turns = std::fmod(radians, turn) / turn;
  if (turns < 0.0f)
    turns += 1.0f;

  return static_cast<std::uint16_t>(std::lround(turns * 65536.0f) & 0xFFFF);
}

/**
 * @brief Decodes an angle encoded by encodeRotation().
 * @param encoded Encoded value.
 * @return The angle in radians, in the range [0, 2 pi).
 */
inline float decodeRotation(be16 encoded) {
  return static_cast<float>(encoded.get()) / 65536.0f * 2.0f *
         std::numbers::pi_v<float>;
}

/** @} */
} // namespace rtype::protocol
