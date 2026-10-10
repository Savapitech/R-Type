#pragma once
#include <concepts>
#include <span>
#include <cstdint>

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
        bool keyPressed[static_cast<std::size_t>(Key::KeyCount)]{};
        bool keyReleased[static_cast<std::size_t>(Key::KeyCount)]{};

        bool mouseButtonPressed[3]{};
        bool mouseButtonReleased[3]{};
        int mouseX{0};
        int mouseY{0};

        bool gamepadButtonPressed[4][static_cast<std::size_t>(GamepadButton::ButtonCount)]{};
        bool gamepadButtonReleased[4][static_cast<std::size_t>(GamepadButton::ButtonCount)]{};
        float gamepadAxis[4][static_cast<std::size_t>(GamepadAxis::AxisCount)]{};
        bool gamepadConnected[4]{};

        [[nodiscard]] bool isKeyPressed(Key key) const {
            if (key == Key::Unknown) return false;
            return keyPressed[static_cast<std::size_t>(key)];
        }

        [[nodiscard]] bool isKeyReleased(Key key) const {
            if (key == Key::Unknown) return false;
            return keyReleased[static_cast<std::size_t>(key)];
        }

        [[nodiscard]] bool isMouseButtonPressed(MouseButton button) const {
            return mouseButtonPressed[static_cast<std::size_t>(button)];
        }

        [[nodiscard]] bool isMouseButtonReleased(MouseButton button) const {
            return mouseButtonReleased[static_cast<std::size_t>(button)];
        }

        [[nodiscard]] int getMouseX() const { return mouseX; }
        [[nodiscard]] int getMouseY() const { return mouseY; }

        [[nodiscard]] bool isGamepadButtonPressed(int joystickId, GamepadButton button) const {
            if (joystickId < 0 || joystickId >= 4 || button == GamepadButton::Unknown) return false;
            return gamepadButtonPressed[joystickId][static_cast<std::size_t>(button)];
        }

        [[nodiscard]] bool isGamepadButtonReleased(int joystickId, GamepadButton button) const {
            if (joystickId < 0 || joystickId >= 4 || button == GamepadButton::Unknown) return false;
            return gamepadButtonReleased[joystickId][static_cast<std::size_t>(button)];
        }

        [[nodiscard]] float getGamepadAxis(int joystickId, GamepadAxis axis) const {
            if (joystickId < 0 || joystickId >= 4) return 0.0f;
            return gamepadAxis[joystickId][static_cast<std::size_t>(axis)];
        }

        [[nodiscard]] bool isGamepadConnected(int joystickId) const {
            if (joystickId < 0 || joystickId >= 4) return false;
            return gamepadConnected[joystickId];
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