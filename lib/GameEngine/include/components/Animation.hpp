#pragma once
#include <cstdint>

namespace component {
struct Animation {
  float frameDuration{0.1f};
  float elapsedTime{0.0f};

  uint16_t currentFrame{0};
  uint16_t totalFrames{1};

  uint16_t frameWidth{0};
  uint16_t frameHeight{0};

  uint16_t startX{0};
  uint16_t startY{0};

  uint16_t columns{0};
  uint16_t paddingX{0};
  uint16_t paddingY{0};

  bool loop{true};
  bool playing{true};
};
} // namespace component