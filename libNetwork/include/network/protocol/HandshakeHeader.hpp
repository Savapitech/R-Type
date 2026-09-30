#pragma once

#include <cstddef>
#include <cstdint>

constexpr std::uint32_t MAGIC = 0x52545950;
constexpr std::uint8_t VERSION = 1;

enum class HandshakeType : std::uint8_t {
  Hello = 1,
  Welcome = 2,
  Rejected = 3,
};

struct HandshakeHeader {
  std::uint32_t magic = MAGIC;
  std::uint8_t version = VERSION;
  HandshakeType type = HandshakeType::Hello;

  static constexpr std::size_t HEADER_SIZE = 6; 
};
