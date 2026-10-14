#pragma once

#include "Logger/Logger.hpp"
#include "components/SoundRequest.hpp"
#include "systems/AudioManagerConcept.hpp"
#include <ISystem.hpp>

namespace rtk::systems {

template <ecs::concepts::IsAudioManager AudioType>
class AudioSystem final : public rtk::ecs::ISystem {
private:
  AudioType &_audioManager;

public:
  AudioSystem(AudioType &audioManager) : _audioManager(audioManager) {}
  ~AudioSystem() override = default;

  void onStart(rtk::ecs::Registry &) override { LOG_INFO("Load Audio System"); }

  void update(rtk::ecs::Registry &reg, float) override {
    _audioManager.update();
  }

  void onStop(rtk::ecs::Registry &) override {}
};

} // namespace rtk::systems