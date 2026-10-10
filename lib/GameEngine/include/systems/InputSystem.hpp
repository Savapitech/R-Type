#pragma once

#include <stdexcept>
#include <functional>

#include <ISystem.hpp>

#include "systems/RenderConcept.hpp"
#include "inputAction/InputAction.hpp"

namespace rtk::systems
{
    template <ecs::concepts::IsRenderWindow WindowType>
    class InputSystem final : public rtk::ecs::ISystem
    {
        public:
            InputSystem(std::reference_wrapper<WindowType> window, rtk::InputState& inputState)
                : _window(window.get()), _inputState(inputState) {};

            ~InputSystem() override = default;

            void onStart(rtk::ecs::Registry&) override
            {
                LOG_DEBUG("Load Input System");
            }

            void update([[maybe_unused]] rtk::ecs::Registry& reg, float) override
            {
                _inputState.actions = InputAction::NoneAction;

                if (!_window.pollEvents(_event))
                    _inputState.closeRequested = true;

                if (_event.isKeyPressed(ecs::concepts::Key::Up) || _event.getGamepadAxis(0, ecs::concepts::GamepadAxis::LeftY) < -0.30f){
                    _inputState.actions |= InputAction::Up;
                    LOG_DEBUG("Key [Up] pressed.");
                }
                if (_event.isKeyPressed(ecs::concepts::Key::Down) || _event.getGamepadAxis(0, ecs::concepts::GamepadAxis::LeftY) > 0.30f){
                    LOG_DEBUG("Key [Down] pressed.");
                    _inputState.actions |= InputAction::Down;
                }
                if (_event.isKeyPressed(ecs::concepts::Key::Left )|| _event.getGamepadAxis(0, ecs::concepts::GamepadAxis::LeftX) < -0.30f){
                    LOG_DEBUG("Key [Left] pressed.");
                    _inputState.actions |= InputAction::Left;
                }
                if (_event.isKeyPressed(ecs::concepts::Key::Right) || _event.getGamepadAxis(0, ecs::concepts::GamepadAxis::LeftX) > 0.30f){
                    LOG_DEBUG("Key [Right] pressed.");
                    _inputState.actions |= InputAction::Right;
                }
                if (_event.isKeyPressed(ecs::concepts::Key::Space) || _event.isGamepadButtonPressed(0, ecs::concepts::GamepadButton::R2)){
                    LOG_DEBUG("Key [Shoot] pressed.");
                    _inputState.actions |= InputAction::Shoot;
                }
                if (_event.isKeyPressed(ecs::concepts::Key::Escape) ){
                    LOG_DEBUG("Key [Close] pressed.");
                    _inputState.closeRequested = true;
                }
                if (_event.isKeyPressed(ecs::concepts::Key::Enter) || _event.isGamepadButtonPressed(0, ecs::concepts::GamepadButton::Cross)) {
                    LOG_DEBUG("Key [Confirm] pressed.");
                    _inputState.actions |= rtk::InputAction::Confirm;
                }
                if ((_event.isKeyPressed(ecs::concepts::Key::F3) && _event.isKeyReleased(ecs::concepts::Key::B)) || _event.isGamepadButtonPressed(0, ecs::concepts::GamepadButton::Cross)) {
                    LOG_DEBUG("Key [Debug] pressed.");
                    _inputState.debug = !_inputState.debug;
                }
            }

            void onStop(rtk::ecs::Registry&) override
            {
            }

        private:
            ecs::concepts::Event _event;
            WindowType& _window;
            rtk::InputState& _inputState;
    };
}