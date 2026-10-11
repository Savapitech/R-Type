#pragma once

#include <vector>
#include <string>

#include <audio/soundManager.hpp>

namespace rtype::client {

class RtkAudioAdapter {
private:
    rtk::SoundManager _engineManager;

    std::vector<rtk::Sound> _sounds;

public:
    RtkAudioAdapter() = default;
    ~RtkAudioAdapter() = default;

    size_t addSound(const std::string& path) {
        rtk::Sound newSound = _engineManager.addSound(path);

        if (!newSound.isValid()) {
            return static_cast<size_t>(-1);
        }

        _sounds.push_back(newSound);
        return _sounds.size() - 1;
    }

    void playSound(size_t id) {
        if (id < _sounds.size()) {
            _engineManager.playSound(_sounds[id]);
        }
    }

    void removeSound(size_t id) {
        if (id < _sounds.size()) {
            _engineManager.removeSound(_sounds[id]);
        }
    }

    void update() {
        _engineManager.update();
    }
};

} // namespace rtype::client