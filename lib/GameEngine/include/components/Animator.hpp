#pragma once
#include <Registry.hpp>
#include <array>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>

namespace component {
enum class animState : uint8_t {
  IDLE = 0,
  UP = 1,
  DOWN = 2,
  DIE = 3,
  SHOOT = 4,
  COUNT = 5,
  NONE = 255
};
struct animationClip {
  uint16_t startX{0};
  uint16_t startY{0};
  uint16_t totalFrames{1};
  float frameDuration{0.1f};
  bool loop{true};
  bool reverse{false};
  uint16_t frameWidth{0};
  uint16_t frameHeight{0};
};
struct animator {
  animState currentState{animState::IDLE};
  animState previousState{animState::NONE};
  std::array<animationClip, static_cast<std::size_t>(animState::COUNT)> clips{};
};

inline animState stringToAnimState(const std::string &stateStr) {
  if (stateStr == "UP")
    return animState::UP;
  if (stateStr == "DOWN")
    return animState::DOWN;
  if (stateStr == "DIE")
    return animState::DIE;
  if (stateStr == "SHOOT")
    return animState::SHOOT;
  return animState::IDLE;
}

inline void animatorFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                             const nlohmann::json &data) {
  component::animator anim{};

  if (data.contains("currentState")) {
    if (data["currentState"].is_string()) {
      anim.currentState =
          stringToAnimState(data["currentState"].get<std::string>());
    } else if (data["currentState"].is_number()) {
      anim.currentState =
          static_cast<animState>(data["currentState"].get<int>());
    }
  }
  anim.previousState = animState::NONE;

  if (data.contains("clips") && data["clips"].is_object()) {
    for (const auto &[stateName, clipData] : data["clips"].items()) {
      animState state = stringToAnimState(stateName);
      std::size_t index = static_cast<std::size_t>(state);

      anim.clips[index].startX = clipData.value<uint16_t>("startX", 0);
      anim.clips[index].startY = clipData.value<uint16_t>("startY", 0);
      anim.clips[index].totalFrames =
          clipData.value<uint16_t>("totalFrames", 1);
      anim.clips[index].frameDuration =
          clipData.value<float>("frameDuration", 0.1f);
      anim.clips[index].loop = clipData.value("loop", true);
      anim.clips[index].reverse = clipData.value("reverse", false);
      anim.clips[index].frameWidth = clipData.value<uint16_t>("frameWidth", 0);
      anim.clips[index].frameHeight =
          clipData.value<uint16_t>("frameHeight", 0);
    }
  }

  reg.get_components<component::animator>().insert_at(entity, anim);
}

} // namespace component