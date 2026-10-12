#pragma once
#include <cstdint>
#include <array>
#include "systems/RenderConcept.hpp"

namespace rtk::Input {

    struct AxisData
    {
        ecs::concepts::GamepadAxis axis;
        float validationPression;
    };

    struct InputBinding {
        enum class Type : uint8_t { NoneAction, Keyboard, GamepadButton, GamepadAxis, Mouse };

        Type type{Type::NoneAction};

        union {
            ecs::concepts::Key key;
            ecs::concepts::GamepadButton button;
            ecs::concepts::MouseButton mouse;
            AxisData axisData;
        };

        InputBinding() : type(Type::NoneAction) {}
    };

    class InputMapping {
    public:
        static constexpr uint8_t MAX_ACTIONS = 32;
        static constexpr uint8_t BINDINGS_PER_ACTION = 4;
    private:
        std::array<InputBinding, MAX_ACTIONS * BINDINGS_PER_ACTION> _bindings;

    public:
        void bindKey(uint8_t bitIndex, ecs::concepts::Key key) {
            addBinding(bitIndex, [key](InputBinding& b) {
                b.type = InputBinding::Type::Keyboard;
                b.key = key;
            });
        }

        void bindGamepad(uint8_t bitIndex, ecs::concepts::GamepadButton button) {
            addBinding(bitIndex, [button](InputBinding& b) {
                b.type = InputBinding::Type::GamepadButton;
                b.button = button;
            });
        }

        void bindMouse(uint8_t bitIndex, ecs::concepts::MouseButton mouseButton) {
            addBinding(bitIndex, [mouseButton](InputBinding& b) {
                b.type = InputBinding::Type::Mouse;
                b.mouse = mouseButton;
            });
        }

        void bindGamepadAxis(uint8_t bitIndex, ecs::concepts::GamepadAxis axis, float validationPression) {
            addBinding(bitIndex, [axis, validationPression](InputBinding& b) {
                b.type = InputBinding::Type::GamepadAxis;
                b.axisData.axis = axis;
                b.axisData.validationPression = validationPression;
            });
        }

        const std::array<InputBinding, MAX_ACTIONS * BINDINGS_PER_ACTION>& getBindings() const {
            return _bindings;
        }

        private:
            template<typename Func>
            void addBinding(uint8_t bitIndex, Func assignFunc) {
                if (bitIndex >= MAX_ACTIONS) return;

                size_t startIndex = bitIndex * BINDINGS_PER_ACTION;
                for (size_t i = 0; i < BINDINGS_PER_ACTION; ++i) {
                    if (_bindings[startIndex + i].type == InputBinding::Type::NoneAction) {
                        assignFunc(_bindings[startIndex + i]);
                        return;
                    }
                }
                LOG_WARN("No free slots left for action bit " + std::to_string(bitIndex));
            }
    };
}