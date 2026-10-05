#pragma once

#include <cstdint>

namespace rtk
{
    enum class InputAction : std::uint8_t
    {
        None  = 0,
        Up    = 1 << 0,
        Down  = 1 << 1,
        Left  = 1 << 2,
        Right = 1 << 3,
        Shoot = 1 << 4
    };

    constexpr InputAction operator|(InputAction left, InputAction right)
    {
        return static_cast<InputAction>(static_cast<std::uint8_t>(left) | static_cast<std::uint8_t>(right));
    }

    constexpr InputAction& operator|=(InputAction& left, InputAction right) {
        left = left | right;
        return left;
    }

    constexpr bool hasAction(InputAction actions, InputAction action) {
        return (static_cast<std::uint8_t>(actions) & static_cast<std::uint8_t>(action)) != 0;
    }

    struct InputState
    {
        InputAction actions = InputAction::None;
        bool closeRequested = false;
    };
}