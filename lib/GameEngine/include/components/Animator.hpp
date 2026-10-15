#pragma once

#include <Registry.hpp>
#include <array>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>

namespace component {
/**
 * @addtogroup api_engine
 * @{
 */

/**
 * @brief Animation states an animator can switch between.
 */
enum class animState : uint8_t {
  IDLE = 0,  ///< Default state.
  UP = 1,    ///< Moving up.
  DOWN = 2,  ///< Moving down.
  DIE = 3,   ///< Dying.
  SHOOT = 4, ///< Shooting.
  COUNT = 5, ///< Number of states.
  NONE = 255 ///< No state, forces the next clip to load.
};

/**
 * @brief Sprite sheet frames played for one animation state.
 */
struct animationClip {
  uint16_t startX{0};        ///< X of the first frame in the sheet.
  uint16_t startY{0};        ///< Y of the first frame in the sheet.
  uint16_t totalFrames{1};   ///< Number of frames.
  float frameDuration{0.1f}; ///< Time per frame, in seconds.
  bool loop{true};           ///< Restarts after the last frame.
  bool reverse{false};       ///< Plays the frames backwards.
  uint16_t frameWidth{0};    ///< Frame width, 0 keeps the current one.
  uint16_t frameHeight{0};   ///< Frame height, 0 keeps the current one.
};

/**
 * @brief State machine that picks the Animation clip of an entity.
 */
struct animator {
  animState currentState{animState::IDLE};  ///< State to display.
  animState previousState{animState::NONE}; ///< State whose clip is loaded.
  /// One clip per state.
  std::array<animationClip, static_cast<std::size_t>(animState::COUNT)> clips{};
};

/**
 * @brief Converts a state name to an animState.
 * @param stateStr State name, such as `UP` or `DIE`.
 * @return The matching state, or IDLE if the name is unknown.
 */
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

/**
 * @brief Adds an animator to an entity from its blueprint data.
 * @param reg Registry owning the entity.
 * @param entity Target entity.
 * @param data JSON object with `currentState` and a `clips` object per state.
 */
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

/** @} */
} // namespace component
