#pragma once
#include <cstdint>
#include <array>
#include "systems/RenderConcept.hpp"

namespace rtk::Input {
    class InputMapping {
    private:
        std::array<ecs::concepts::Key, 32> _keyBindings;

    public:
        InputMapping() {
            _keyBindings.fill(ecs::concepts::Key::Unknown);
        }
        void bindKey(uint8_t bitIndex, ecs::concepts::Key key) {
            if (bitIndex < 32) {
                _keyBindings[bitIndex] = key;
            }
        }

        const std::array<ecs::concepts::Key, 32>& getBindings() const {
            return _keyBindings;
        }
    };
}