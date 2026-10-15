#pragma once

#include <concepts>
#include <string>

namespace rtk::ecs::concepts {
/**
 * @addtogroup api_engine
 * @{
 */

/**
 * @brief Requirements for the audio backend used by rtk::systems::AudioSystem.
 *
 * The backend loads sounds with `addSound(path)`, which returns an id, plays
 * and removes them by id, and is ticked once per frame with `update()`.
 *
 * @tparam T Candidate audio backend.
 */
template <typename T>
concept IsAudioManager =
    requires(T manager, const std::string &path, size_t soundId) {
      { manager.addSound(path) } -> std::same_as<size_t>;

      { manager.playSound(soundId) } -> std::same_as<void>;
      { manager.removeSound(soundId) } -> std::same_as<void>;
      { manager.update() } -> std::same_as<void>;
    };

/** @} */
} // namespace rtk::ecs::concepts
