#pragma once

#include <string>
#include <vector>

#include <audio/soundManager.hpp>

namespace rtype::client {
/**
 * @addtogroup api_client
 * @{
 */

/**
 * @brief Adapts the sound manager of the graphical library to the
 * IsAudioManager concept.
 */
class RtkAudioAdapter {
private:
  rtk::SoundManager _engineManager;

  std::vector<rtk::Sound> _sounds;

public:
  RtkAudioAdapter() = default;
  ~RtkAudioAdapter() = default;

  /**
   * @brief Loads a sound file.
   * @param path Path of the sound file.
   * @return The sound id, or `size_t(-1)` if loading failed.
   */
  size_t addSound(const std::string &path) {
    rtk::Sound newSound = _engineManager.addSound(path);

    if (!newSound.isValid()) {
      return static_cast<size_t>(-1);
    }

    _sounds.push_back(newSound);
    return _sounds.size() - 1;
  }

  /**
   * @brief Plays a loaded sound, unknown ids are ignored.
   * @param id Sound id returned by addSound().
   */
  void playSound(size_t id) {
    if (id < _sounds.size()) {
      _engineManager.playSound(_sounds[id]);
    }
  }

  /**
   * @brief Unloads a sound, unknown ids are ignored.
   * @param id Sound id returned by addSound().
   */
  void removeSound(size_t id) {
    if (id < _sounds.size()) {
      _engineManager.removeSound(_sounds[id]);
    }
  }

  /**
   * @brief Ticks the underlying sound manager.
   */
  void update() { _engineManager.update(); }
};

/** @} */
} // namespace rtype::client
