#pragma once
#include <concepts>
#include <span>
#include <cstdint>
#include <bitset>

#include "../utils/vec2.hpp"

namespace rtk::ecs::concepts
{
    enum class Key : uint16_t {
        Unknown = 0,
        A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
        Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
        Escape, LControl, LShift, LAlt, LSystem, RControl, RShift, RAlt, RSystem, Menu,
        LBracket, RBracket, Semicolon, Comma, Period, Quote, Slash, Backslash, Tilde, Equal, Hyphen,
        Space, Enter, Backspace, Tab, PageUp, PageDown, End, Home, Insert, Delete,
        Add, Subtract, Multiply, Divide, Left, Right, Up, Down,
        Numpad0, Numpad1, Numpad2, Numpad3, Numpad4, Numpad5, Numpad6, Numpad7, Numpad8, Numpad9,
        F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12, F13, F14, F15,
        KeyCount = 256
    };

    enum class MouseButton : uint16_t { Left = 0, Right = 1, Middle = 2, ButtonCount = 3 };

    enum class GamepadButton : uint16_t {
        Unknown = 0, A, B, X, Y, Cross = A, Circle = B, Square = X, Triangle = Y,
        L1, R1, L2, R2, Select, Start, L3, R3, DpadUp, DpadDown, DpadLeft, DpadRight, Home,
        ButtonCount
    };

    enum class GamepadAxis : uint16_t { LeftX = 0, LeftY, RightX, RightY, L2, R2, AxisCount };

    class Event {
    public:
        void* userData{nullptr};
        bool (*_isKeyPressed)(void*, Key){nullptr};
        bool (*_isKeyReleased)(void*, Key){nullptr};
        bool (*_isMouseButtonPressed)(void*, MouseButton){nullptr};
        bool (*_isMouseButtonReleased)(void*, MouseButton){nullptr};
        int (*_getMouseX)(void*){nullptr};
        int (*_getMouseY)(void*){nullptr};
        bool (*_isGamepadButtonPressed)(void*, int, GamepadButton){nullptr};
        bool (*_isGamepadButtonReleased)(void*, int, GamepadButton){nullptr};
        float (*_getGamepadAxis)(void*, int, GamepadAxis){nullptr};
        bool (*_isGamepadConnected)(void*, int){nullptr};

        [[nodiscard]] bool isKeyPressed(Key key) const {
            return _isKeyPressed ? _isKeyPressed(userData, key) : false;
        }

        [[nodiscard]] bool isKeyReleased(Key key) const {
            return _isKeyReleased ? _isKeyReleased(userData, key) : false;
        }

        [[nodiscard]] bool isMouseButtonPressed(MouseButton button) const {
            return _isMouseButtonPressed ? _isMouseButtonPressed(userData, button) : false;
        }

        [[nodiscard]] bool isMouseButtonReleased(MouseButton button) const {
            return _isMouseButtonReleased ? _isMouseButtonReleased(userData, button) : false;
        }

        [[nodiscard]] int getMouseX() const {
            return _getMouseX ? _getMouseX(userData) : 0;
        }

        [[nodiscard]] int getMouseY() const {
            return _getMouseY ? _getMouseY(userData) : 0;
        }

        [[nodiscard]] bool isGamepadButtonPressed(int joystickId, GamepadButton button) const {
            return _isGamepadButtonPressed ? _isGamepadButtonPressed(userData, joystickId, button) : false;
        }

        [[nodiscard]] bool isGamepadButtonReleased(int joystickId, GamepadButton button) const {
            return _isGamepadButtonReleased ? _isGamepadButtonReleased(userData, joystickId, button) : false;
        }

        [[nodiscard]] float getGamepadAxis(int joystickId, GamepadAxis axis) const {
            return _getGamepadAxis ? _getGamepadAxis(userData, joystickId, axis) : 0.0f;
        }

        [[nodiscard]] bool isGamepadConnected(int joystickId) const {
            return _isGamepadConnected ? _isGamepadConnected(userData, joystickId) : false;
        }
    };

    struct TextureRectU16 {
        uint16_t x;
        uint16_t y;
        uint16_t width;
        uint16_t height;
    };

    struct ColorRGBA8 {
        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint8_t a;
    };

    struct alignas(64) SpriteData {
        utils::vec2 position;
        utils::vec2 scale{1.f, 1.f};
        utils::vec2 size;
        utils::vec2 origin;

        TextureRectU16 textureRect{};
        ColorRGBA8 color{};
        float rotation{0.f};

        uint32_t textureId{0};
        int32_t layer{0};
        uint32_t flags{0};
        uint32_t reserved{0};
    };

    template <typename T>
    concept IsRenderWindow = requires(T window, Event& event, ColorRGBA8 color, std::span<const SpriteData> sprites)
    {
        { window.pollEvents(event) } -> std::same_as<bool>;
        { window.beginFrame(color) } -> std::same_as<bool>;
        { window.draw(sprites) } -> std::same_as<void>;
        { window.endFrame() } -> std::same_as<void>;
    };
}