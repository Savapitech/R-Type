#pragma once
#include <array>
#include <cstdint>

namespace component {
enum class animState : uint8_t {
  IDLE = 0,
  UP = 1,
  DOWN = 2,
  DIE = 3,
  SHOOT = 4,
  COUNT = 5
};
struct animationClip {
  uint16_t startY{0};
  uint16_t totalFrames{1};
  bool loop{true};
};
struct animator {
  animState currentState{animState::IDLE};
  animState previousState{animState::IDLE};
  std::array<animationClip, static_cast<std::size_t>(animState::COUNT)> clips{};
};

} // namespace component