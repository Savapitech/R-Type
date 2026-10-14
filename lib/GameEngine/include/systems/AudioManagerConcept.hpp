#pragma once

#include <concepts>
#include <string>

namespace rtk::ecs::concepts {

template <typename T>
concept IsAudioManager =
    requires(T manager, const std::string &path, size_t soundId) {
      { manager.addSound(path) } -> std::same_as<size_t>;

      { manager.playSound(soundId) } -> std::same_as<void>;
      { manager.removeSound(soundId) } -> std::same_as<void>;
      { manager.update() } -> std::same_as<void>;
    };

} // namespace rtk::ecs::concepts