#pragma once

#include "Logger/Logger.hpp"
#include "components/SoundRequest.hpp"
#include "systems/AudioManagerConcept.hpp"
#include <ISystem.hpp>

namespace rtk::systems {
/**
 * @addtogroup api_engine
 * @{
 */

/**
 * @brief Ticks the audio backend once per frame.
 * @tparam AudioType Backend satisfying IsAudioManager.
 */
template <ecs::concepts::IsAudioManager AudioType>
class AudioSystem final : public rtk::ecs::ISystem {
private:
  AudioType &_audioManager;

public:
  /**
   * @brief Creates the system.
   * @param audioManager Backend to tick.
   */
  AudioSystem(AudioType &audioManager) : _audioManager(audioManager) {}
  ~AudioSystem() override = default;

  /**
   * @brief Logs that the system is loaded.
   */
  void onStart(rtk::ecs::Registry &) override { LOG_INFO("Load Audio System"); }

  /**
   * @brief Calls `update()` on the backend.
   */
  void update(rtk::ecs::Registry &reg, float) override {
    _audioManager.update();
  }

  /**
   * @brief Does nothing.
   */
  void onStop(rtk::ecs::Registry &) override {}
};

/** @} */
} // namespace rtk::systems
