#pragma once

#include <ISystem.hpp>
#include "systems/AudioManagerConcept.hpp"
#include "components/SoundRequest.hpp"
#include "Logger/Logger.hpp"

namespace rtk::systems {

template <ecs::concepts::IsAudioManager AudioType>
class AudioSystem final : public rtk::ecs::ISystem {
private:
    AudioType& _audioManager;

public:
    AudioSystem(AudioType& audioManager) : _audioManager(audioManager) {}
    ~AudioSystem() override = default;

    void onStart(rtk::ecs::Registry &) override {
        LOG_INFO("Load Audio System");
    }

    void update(rtk::ecs::Registry &reg, float) override {
        _audioManager.update();

        auto view = reg.view<component::SoundRequest>();
        auto& soundRequests = reg.get_components<component::SoundRequest>();

        for (auto entity : view) {
            auto *request = soundRequests.get(entity);

            if (request) {
                _audioManager.playSound(request->soundId);
                soundRequests.erase(entity);
            }
        }
    }

    void onStop(rtk::ecs::Registry &) override {}
};

} // namespace rtk::systems