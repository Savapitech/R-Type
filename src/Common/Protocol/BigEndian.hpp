#pragma once

/**
 * @addtogroup api_protocol
 * @{
 */

#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>

namespace rtype::protocol {
/**
 * @addtogroup api_protocol
 * @{
 */

namespace detail {
/**
 * @addtogroup api_protocol
 * @{
 */

template <std::unsigned_integral T> constexpr T byteswap(T value) {
  if constexpr (sizeof(T) == 1) {
    return value;
  } else {
    T result = 0;
    for (std::size_t i = 0; i < sizeof(T); ++i) {
      result = static_cast<T>((result << 8) | (value & 0xFFu));
      value = static_cast<T>(value >> 8);
    }
    return result;
  }
}

template <std::unsigned_integral T> constexpr T hostToBig(T value) {
  if constexpr (std::endian::native == std::endian::little)
    return byteswap(value);
  else
    return value;
}

/** @} */
} // namespace detail

/**
 * @brief Unsigned integer stored as big-endian bytes, with an alignment of 1.
 *
 * Structs made only of these fields never contain padding, so they can be
 * copied byte for byte to and from the wire on any host.
 *
 * @tparam T Unsigned integer type that is stored.
 */
template <std::unsigned_integral T> struct BigEndian {
  std::array<std::uint8_t, sizeof(T)> bytes{}; ///< Value in big-endian order.

  /**
   * @brief Creates a zero value.
   */
  constexpr BigEndian() = default;

  /**
   * @brief Stores a host value in big-endian order.
   * @param value Value in host byte order.
   */
  constexpr BigEndian(T value)
      : bytes(std::bit_cast<std::array<std::uint8_t, sizeof(T)>>(
            detail::hostToBig(value))) {}

  /**
   * @brief Returns the value in host byte order.
   * @return The decoded value.
   */
  constexpr T get() const { return detail::hostToBig(std::bit_cast<T>(bytes)); }

  /**
   * @brief Converts to the value in host byte order.
   */
  constexpr operator T() const { return get(); }
};

/// 16-bit unsigned integer stored in big-endian order.
using be16 = BigEndian<std::uint16_t>;
/// 32-bit unsigned integer stored in big-endian order.
using be32 = BigEndian<std::uint32_t>;

/** @} */
} // namespace rtype::protocol

/** @} */
