#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>

#include <Registry.hpp>

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
  bool reverse{false};
};

inline void animationFromJson(rtk::ecs::Registry& reg, std::size_t entity, const nlohmann::json& data) {
    component::Animation animation{};
    animation.frameDuration = data.value("frameDuration", 0.1f);
    animation.elapsedTime = data.value("elapsedTime", 0.0f);
    animation.currentFrame = data.value("currentFrame", static_cast<std::uint16_t>(0));
    animation.totalFrames = data.value("totalFrames", static_cast<std::uint16_t>(1));
    animation.frameWidth = data.value("frameWidth", static_cast<std::uint16_t>(0));
    animation.frameHeight = data.value("frameHeight", static_cast<std::uint16_t>(0));
    animation.startX = data.value("startX", static_cast<std::uint16_t>(0));
    animation.startY = data.value("startY", static_cast<std::uint16_t>(0));
    animation.columns = data.value("columns", static_cast<std::uint16_t>(0));
    animation.paddingX = data.value("paddingX", static_cast<std::uint16_t>(0));
    animation.paddingY = data.value("paddingY", static_cast<std::uint16_t>(0));
    animation.loop = data.value("loop", true);
    animation.playing = data.value("playing", true);
    animation.reverse = data.value("reverse", false);

    reg.get_components<component::Animation>().insert_at(entity, animation);
}

} // namespace component