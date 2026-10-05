#pragma once

#include <cstddef>

inline constexpr std::size_t MAX_PACKET_SIZE = 576;
inline constexpr std::size_t IP_HEADER_SIZE = 20;
inline constexpr std::size_t UDP_HEADER_SIZE = 8;
inline constexpr std::size_t MAX_PAYLOAD_SIZE =
  MAX_PACKET_SIZE - IP_HEADER_SIZE - UDP_HEADER_SIZE;
inline constexpr std::size_t PACKET_HEADER_SIZE = 16;
inline constexpr std::size_t MAX_MESSAGE_SIZE = MAX_PAYLOAD_SIZE - PACKET_HEADER_SIZE;
