#include "audio/IAudioEngine.hpp"
#include <audio/soundManager.hpp>
#include <iostream>
#include <vector>

namespace {

class AudioEngineRtk final : public rtk::audio::IAudioEngine {
public:
  AudioEngineRtk() {
    std::cout << "[RTK Audio Plugin] Initialized." << std::endl;
  }

  ~AudioEngineRtk() override {
    std::cout << "[RTK Audio Plugin] Destroyed." << std::endl;
  }

  std::size_t loadSound(const std::string &path) override {
    rtk::Sound sound = _manager.addSound(path);
    if (!sound.isValid()) {
      std::cerr << "[RTK Audio Plugin] Failed to load sound: " << path << std::endl;
      return static_cast<std::size_t>(-1);
    }
    _sounds.push_back(sound);
    return _sounds.size() - 1;
  }

  void playSound(std::size_t soundId) override {
    if (soundId < _sounds.size()) {
      _manager.playSound(_sounds[soundId]);
    }
  }

  void stopSound(std::size_t soundId) override {
    if (soundId < _sounds.size()) {
      _manager.removeSound(_sounds[soundId]);
    }
  }

  void update() override {
    _manager.update();
  }

  [[nodiscard]] const char *getName() const noexcept override {
    return "RTK Graphical Audio Plugin";
  }

private:
  rtk::SoundManager _manager{};
  std::vector<rtk::Sound> _sounds{};
};

} // namespace

extern "C" {

RTK_PLUGIN_EXPORT rtk::audio::IAudioEngine *createAudioEngine() {
  return new AudioEngineRtk();
}

RTK_PLUGIN_EXPORT void destroyAudioEngine(rtk::audio::IAudioEngine *engine) {
  delete engine;
}

}
