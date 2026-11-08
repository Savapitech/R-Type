#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Logger/Logger.hpp"
#include <miniaudio.h>

namespace rtype::client {
/**
 * @addtogroup api_client
 * @{
 */

/**
 * @brief Internal entry tracking an initialized miniaudio sound.
 */
struct SoundEntry {
  ma_sound sound{};     ///< Low-level miniaudio sound object.
  std::string path{};   ///< File path to the loaded audio asset.
  bool isLoaded{false}; ///< Whether the sound has been initialized and loaded.

  SoundEntry() = default;
  ~SoundEntry() {
    if (isLoaded) {
      ma_sound_uninit(&sound);
      isLoaded = false;
    }
  }

  SoundEntry(const SoundEntry &) = delete;
  SoundEntry &operator=(const SoundEntry &) = delete;
  SoundEntry(SoundEntry &&) = delete;
  SoundEntry &operator=(SoundEntry &&) = delete;
};

/**
 * @brief Sound and music manager adapting miniaudio to the IsAudioManager
 * concept.
 *
 * Supports loading, playing, stopping dedicated sounds, music streaming,
 * looping, and global mute/volume controls.
 */
class RtkAudioAdapter {
private:
  ma_engine _engine{};
  bool _engineInitialized{false};
  bool _muted{false};
  float _masterVolume{1.0f};
  float _musicVolume{0.8f};

  std::vector<std::unique_ptr<SoundEntry>> _sounds;
  std::unique_ptr<SoundEntry> _musicEntry{nullptr};

public:
  RtkAudioAdapter() {
    if (ma_engine_init(NULL, &_engine) == MA_SUCCESS) {
      _engineInitialized = true;
    } else {
      LOG_ERROR("Failed to initialize miniaudio engine");
    }
  }

  ~RtkAudioAdapter() {
    stopAllSounds();
    _sounds.clear();
    _musicEntry.reset();
    if (_engineInitialized) {
      ma_engine_uninit(&_engine);
      _engineInitialized = false;
    }
  }

  RtkAudioAdapter(const RtkAudioAdapter &) = delete;
  RtkAudioAdapter &operator=(const RtkAudioAdapter &) = delete;

  /**
   * @brief Loads a sound file from disk.
   * @param path Path to the audio file.
   * @return Sound identifier, or size_t(-1) on failure.
   */
  size_t addSound(const std::string &path) {
    if (!_engineInitialized)
      return static_cast<size_t>(-1);

    auto entry = std::make_unique<SoundEntry>();
    entry->path = path;
    const ma_result result = ma_sound_init_from_file(&_engine, path.c_str(), 0,
                                                     NULL, NULL, &entry->sound);

    if (result != MA_SUCCESS) {
      LOG_WARN("Failed to load sound file: " + path);
      return static_cast<size_t>(-1);
    }

    entry->isLoaded = true;
    _sounds.push_back(std::move(entry));
    return _sounds.size() - 1;
  }

  /**
   * @brief Plays a loaded sound by identifier.
   * @param id Identifier returned by addSound().
   */
  void playSound(size_t id) {
    if (_muted || id >= _sounds.size() || !_sounds[id]->isLoaded)
      return;

    ma_sound_seek_to_pcm_frame(&_sounds[id]->sound, 0);
    ma_sound_start(&_sounds[id]->sound);
  }

  /**
   * @brief Stops a specific dedicated sound by identifier.
   * @param id Identifier of the sound to cut.
   */
  void stopSound(size_t id) {
    if (id >= _sounds.size() || !_sounds[id]->isLoaded)
      return;

    ma_sound_stop(&_sounds[id]->sound);
    ma_sound_seek_to_pcm_frame(&_sounds[id]->sound, 0);
  }

  /**
   * @brief Checks if a specific sound is currently playing.
   * @param id Identifier of the sound.
   * @return True if currently playing.
   */
  [[nodiscard]] bool isSoundPlaying(size_t id) const {
    if (id >= _sounds.size() || !_sounds[id]->isLoaded)
      return false;
    return ma_sound_is_playing(&_sounds[id]->sound) == MA_TRUE;
  }

  /**
   * @brief Configures looping for a specific sound.
   * @param id Identifier of the sound.
   * @param loop True to loop endlessly.
   */
  void setSoundLooping(size_t id, bool loop) {
    if (id >= _sounds.size() || !_sounds[id]->isLoaded)
      return;
    ma_sound_set_looping(&_sounds[id]->sound, loop ? MA_TRUE : MA_FALSE);
  }

  /**
   * @brief Sets the volume of a specific sound.
   * @param id Identifier of the sound.
   * @param volume Volume factor (0.0 to 1.0).
   */
  void setSoundVolume(size_t id, float volume) {
    if (id >= _sounds.size() || !_sounds[id]->isLoaded)
      return;
    ma_sound_set_volume(&_sounds[id]->sound, volume);
  }

  /**
   * @brief Stops all active sounds and music.
   */
  void stopAllSounds() {
    for (auto &entry : _sounds) {
      if (entry && entry->isLoaded) {
        ma_sound_stop(&entry->sound);
        ma_sound_seek_to_pcm_frame(&entry->sound, 0);
      }
    }
    stopMusic();
  }

  /**
   * @brief Stops a sound by identifier (satisfies IsAudioManager).
   * @param id Identifier of the sound.
   */
  void removeSound(size_t id) { stopSound(id); }

  /**
   * @brief Plays dedicated background music.
   * @param path Path to the music file.
   * @param loop True to loop endlessly (default).
   */
  void playMusic(const std::string &path, bool loop = true) {
    stopMusic();
    if (!_engineInitialized)
      return;

    _musicEntry = std::make_unique<SoundEntry>();
    _musicEntry->path = path;
    const ma_result result =
        ma_sound_init_from_file(&_engine, path.c_str(), MA_SOUND_FLAG_STREAM,
                                NULL, NULL, &_musicEntry->sound);

    if (result != MA_SUCCESS) {
      LOG_WARN("Failed to load music file: " + path);
      return;
    }

    _musicEntry->isLoaded = true;
    ma_sound_set_looping(&_musicEntry->sound, loop ? MA_TRUE : MA_FALSE);
    ma_sound_set_volume(&_musicEntry->sound, _musicVolume);

    if (!_muted) {
      ma_sound_start(&_musicEntry->sound);
    }
  }

  /**
   * @brief Stops currently playing background music.
   */
  void stopMusic() {
    if (_musicEntry && _musicEntry->isLoaded) {
      ma_sound_stop(&_musicEntry->sound);
      ma_sound_seek_to_pcm_frame(&_musicEntry->sound, 0);
    }
  }

  /**
   * @brief Pauses background music.
   */
  void pauseMusic() {
    if (_musicEntry && _musicEntry->isLoaded) {
      ma_sound_stop(&_musicEntry->sound);
    }
  }

  /**
   * @brief Resumes paused background music.
   */
  void resumeMusic() {
    if (_musicEntry && _musicEntry->isLoaded && !_muted) {
      ma_sound_start(&_musicEntry->sound);
    }
  }

  /**
   * @brief Checks if background music is currently playing.
   */
  [[nodiscard]] bool isMusicPlaying() const {
    if (_musicEntry && _musicEntry->isLoaded) {
      return ma_sound_is_playing(&_musicEntry->sound) == MA_TRUE;
    }
    return false;
  }

  /**
   * @brief Sets whether all sound output is muted.
   * @param muted True to mute.
   */
  void setMuted(bool muted) {
    _muted = muted;
    if (_engineInitialized) {
      ma_engine_set_volume(&_engine, _muted ? 0.0f : _masterVolume);
    }
  }

  /**
   * @brief Checks if sound is currently muted.
   */
  [[nodiscard]] bool isMuted() const noexcept { return _muted; }

  /**
   * @brief Toggles mute state.
   * @return New mute state.
   */
  bool toggleMute() {
    setMuted(!_muted);
    return _muted;
  }

  /**
   * @brief Ticks the audio backend (satisfies IsAudioManager).
   */
  void update() {}
};

/** @} */
} // namespace rtype::client
