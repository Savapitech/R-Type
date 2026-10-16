#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>

#include <Registry.hpp>

namespace component {
/**
 * @addtogroup api_engine
 * @{
 */

/**
 * @brief Sprite sheet animation played by changing the texture rectangle.
 */
struct Animation {
  float frameDuration{0.1f}; ///< Time each frame stays visible, in seconds.
  float elapsedTime{0.0f};   ///< Time spent on the current frame.

  uint16_t currentFrame{0}; ///< Index of the visible frame.
  uint16_t totalFrames{1};  ///< Number of frames in the animation.

  uint16_t frameWidth{0};  ///< Frame width in the sheet, in pixels.
  uint16_t frameHeight{0}; ///< Frame height in the sheet, in pixels.

  uint16_t startX{0}; ///< X of the first frame in the sheet.
  uint16_t startY{0}; ///< Y of the first frame in the sheet.

  uint16_t columns{0};  ///< Frames per row, 0 for a single row.
  uint16_t paddingX{0}; ///< Horizontal gap between frames.
  uint16_t paddingY{0}; ///< Vertical gap between rows.

  bool loop{true};     ///< Restarts after the last frame.
  bool playing{true};  ///< Advances frames while true.
  bool reverse{false}; ///< Plays the frames backwards.
};

/**
 * @brief Adds an Animation to an entity from its blueprint data.
 * @param reg Registry owning the entity.
 * @param entity Target entity.
 * @param data JSON object whose keys match the Animation fields.
 */
inline void animationFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                              const nlohmann::json &data) {
  component::Animation animation{};
  animation.frameDuration = data.value("frameDuration", 0.1f);
  animation.elapsedTime = data.value("elapsedTime", 0.0f);
  animation.currentFrame =
      data.value("currentFrame", static_cast<std::uint16_t>(0));
  animation.totalFrames =
      data.value("totalFrames", static_cast<std::uint16_t>(1));
  animation.frameWidth =
      data.value("frameWidth", static_cast<std::uint16_t>(0));
  animation.frameHeight =
      data.value("frameHeight", static_cast<std::uint16_t>(0));
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

/** @} */
} // namespace component
