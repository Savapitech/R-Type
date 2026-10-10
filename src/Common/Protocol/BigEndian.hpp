#pragma once

#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>

namespace rtype::protocol {
namespace detail {
template <std::unsigned_integral T> constexpr T byteswap(T value) {
  if constexpr (sizeof(T) == 1) {
    return value;
  } else if constexpr (sizeof(T) == 2) {
    return __builtin_bswap16(value);
  } else if constexpr (sizeof(T) == 4) {
    return __builtin_bswap32(value);
  } else {
    return __builtin_bswap64(value);
  }
}

template <std::unsigned_integral T> constexpr T hostToBig(T value) {
  if constexpr (std::endian::native == std::endian::little)
    return byteswap(value);
  else
    return value;
}
} // namespace detail

// Integer stored as big-endian bytes (proto's sction 1)
// Being a plain byte array its alignment is 1: structs made of these
// never contain padding, whatever the field order, and can be copied
// raw on  on any host
template <std::unsigned_integral T> struct BigEndian {
  std::array<std::uint8_t, sizeof(T)> bytes{};

  constexpr BigEndian() = default;

  constexpr BigEndian(T value)
      : bytes(std::bit_cast<std::array<std::uint8_t, sizeof(T)>>(
            detail::hostToBig(value))) {}

  constexpr T get() const { return detail::hostToBig(std::bit_cast<T>(bytes)); }

  constexpr operator T() const { return get(); }
};

using be16 = BigEndian<std::uint16_t>;
using be32 = BigEndian<std::uint32_t>;
} // namespace rtype::protocol
