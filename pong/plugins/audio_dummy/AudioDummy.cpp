#include "audio/IAudioEngine.hpp"
#include <iostream>
#include <vector>

namespace {

class AudioEngineDummy final : public rtk::audio::IAudioEngine {
public:
  AudioEngineDummy() {
    std::cout << "[Dummy Audio Plugin] Initialized." << std::endl;
  }

  ~AudioEngineDummy() override {
    std::cout << "[Dummy Audio Plugin] Destroyed." << std::endl;
  }

  std::size_t loadSound(const std::string &path) override {
    _soundPaths.push_back(path);
    std::cout << "[Dummy Audio Plugin] Registered sound #" << (_soundPaths.size() - 1)
              << ": " << path << std::endl;
    return _soundPaths.size() - 1;
  }

  void playSound(std::size_t soundId) override {
    if (soundId < _soundPaths.size()) {
      std::cout << "\033[1;33m[Dummy Audio Plugin] Playing sound #"
                << soundId << " (" << _soundPaths[soundId] << ")\033[0m" << std::endl;
    }
  }

  void stopSound(std::size_t soundId) override {
    if (soundId < _soundPaths.size()) {
      std::cout << "[Dummy Audio Plugin] Stopped sound #" << soundId << std::endl;
    }
  }

  void update() override {}

  [[nodiscard]] const char *getName() const noexcept override {
    return "Dummy Console Audio Plugin";
  }

private:
  std::vector<std::string> _soundPaths{};
};

} // namespace

extern "C" {

RTK_PLUGIN_EXPORT rtk::audio::IAudioEngine *createAudioEngine() {
  return new AudioEngineDummy();
}

RTK_PLUGIN_EXPORT void destroyAudioEngine(rtk::audio::IAudioEngine *engine) {
  delete engine;
}

}
