#pragma once

#include <cmath>
#include <cstdint>

#include "BigEndian.hpp"

namespace rtype::protocol {
/**
 * @addtogroup api_protocol
 * @{
 */

/**
 * @brief Kind of power up carried by a PowerUpTaken message.
 */
enum class PowerUpKind : std::uint8_t {
  Heal = 0x00,         ///< Restores health.
  Speed = 0x01,        ///< Raises the ship speed, value in pixels per second.
  Shield = 0x02,       ///< Adds health above the maximum.
  WeaponSpread = 0x03, ///< Three way shot, value is the duration in seconds.
  WeaponPlasma = 0x04  ///< Plasma shot, value is the duration in seconds.
};

/**
 * @brief Payload of a PowerUpTaken message, 3 bytes, sent by the server to
 * the player whose ship picked up a power up.
 *
 * | Offset | Size | Field   | Description                                |
 * | -----: | ---: | ------- | ------------------------------------------ |
 * | 0      | 1    | `kind`  | A PowerUpKind value.                       |
 * | 1      | 2    | `value` | Effect strength, in tenths (big endian).   |
 */
struct PowerUpTakenPayload {
  PowerUpKind kind = PowerUpKind::Heal; ///< Kind of the power up.
  be16 value;                           ///< Strength, in tenths of a unit.
};

static_assert(sizeof(PowerUpTakenPayload) == sizeof(PowerUpKind) + sizeof(be16),
              "PowerUpTakenPayload must not contain padding");

/**
 * @brief Checks that the kind is a known value.
 * @param payload Decoded payload.
 * @return True if the payload is valid.
 */
constexpr bool isValid(const PowerUpTakenPayload &payload) {
  return static_cast<std::uint8_t>(payload.kind) <=
         static_cast<std::uint8_t>(PowerUpKind::WeaponPlasma);
}

/**
 * @brief Builds a PowerUpTaken payload.
 * @param kind Kind of the power up.
 * @param value Effect strength, clamped to 0 to 6553.5.
 * @return The payload.
 */
inline PowerUpTakenPayload powerUpTakenPayload(PowerUpKind kind, float value) {
  PowerUpTakenPayload payload;
  payload.kind = kind;
  const float clamped =
      std::isfinite(value) ? std::fmax(0.0f, std::fmin(value, 6553.5f)) : 0.0f;
  payload.value = static_cast<std::uint16_t>(std::lround(clamped * 10.0f));
  return payload;
}

/**
 * @brief Returns the effect strength of a payload.
 * @param payload Decoded payload.
 * @return The value, in the unit of the kind.
 */
inline float powerUpValue(const PowerUpTakenPayload &payload) {
  return static_cast<float>(payload.value.get()) / 10.0f;
}

/** @} */
} // namespace rtype::protocol
