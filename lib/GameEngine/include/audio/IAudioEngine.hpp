#pragma once

#include <cstddef>
#include <string>

namespace rtk::audio {

/**
 * @brief Common interface for audio engine plugins.
 */
class IAudioEngine {
public:
  virtual ~IAudioEngine() = default;

  /**
   * @brief Loads a sound file from disk.
   * @param path File path of the audio asset.
   * @return Identifier of the loaded sound, or static_cast<size_t>(-1) on failure.
   */
  virtual std::size_t loadSound(const std::string &path) = 0;

  /**
   * @brief Plays a loaded sound by its identifier.
   * @param soundId Identifier returned by loadSound.
   */
  virtual void playSound(std::size_t soundId) = 0;

  /**
   * @brief Stops a playing sound if supported.
   * @param soundId Identifier of the sound.
   */
  virtual void stopSound(std::size_t soundId) = 0;

  /**
   * @brief Updates any internal audio states (to be called every frame).
   */
  virtual void update() = 0;

  /**
   * @brief Returns a human-readable name of the audio engine implementation.
   */
  [[nodiscard]] virtual const char *getName() const noexcept = 0;
};

} // namespace rtk::audio

#if defined(_WIN32)
  #define RTK_PLUGIN_EXPORT __declspec(dllexport)
#else
  #define RTK_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

using CreateAudioEngineFn = rtk::audio::IAudioEngine *(*)();
using DestroyAudioEngineFn = void (*)(rtk::audio::IAudioEngine *);
