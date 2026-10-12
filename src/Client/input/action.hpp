#pragma once

#include <cstdint>

namespace rtype {
    enum class Action : uint32_t {
        NoneAction    = 0,
        Up      = 1 << 0,
        Down    = 1 << 1,
        Left    = 1 << 2,
        Right   = 1 << 3,
        Shoot   = 1 << 4,
        Confirm = 1 << 5
    };

    namespace Input {
        [[nodiscard]]
        constexpr bool hasAction(uint32_t engineMask, Action action) {
            return (engineMask & static_cast<uint32_t>(action)) != 0;
        }
    }
}