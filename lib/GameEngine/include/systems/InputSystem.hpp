#pragma once

#include <stdexcept>
#include <vector>

#include <ISystem.hpp>
#include <sprite/spriteData.hpp>
#include <graphical/renderWindow.hpp>

#include "components/Transform.hpp"
#include "inputAction/InputAction.hpp"


namespace rtk::systems
{
    class InputSystem final : public rtk::ecs::ISystem
    {
        public:
            InputSystem(rtk::RenderWindow& window,  rtk::InputState& inputState) : _window(window), _inputState(inputState) {};

            ~InputSystem() override = default;

            void onStart(rtk::ecs::Registry&) override
            {
                LOG_INFO("Load Input System");
            }

            void update([[maybe_unused]] rtk::ecs::Registry& reg, float) override
            {
                _inputState.actions = InputAction::NoneAction;
                if (!_window.pollEvents(_event))
                    throw std::runtime_error("Window closed");

                if (_event.isKeyPressed(rtk::Key::Z)){
                    _inputState.actions |= InputAction::Up;
                    LOG_DEBUG("Key [Up] pressed.");
                }
                if (_event.isKeyPressed(rtk::Key::S)){
                    LOG_DEBUG("Key [Down] pressed.");
                    _inputState.actions |= InputAction::Down;
                }
                if (_event.isKeyPressed(rtk::Key::Q)){
                    LOG_DEBUG("Key [Left] pressed.");
                    _inputState.actions |= InputAction::Left;
                }
                if (_event.isKeyPressed(rtk::Key::D)){
                    LOG_DEBUG("Key [Right] pressed.");
                    _inputState.actions |= InputAction::Right;
                }
                if (_event.isKeyPressed(rtk::Key::Space)){
                    LOG_DEBUG("Key [Shoot] pressed.");
                    _inputState.actions |= InputAction::Shoot;
                }
                if (_event.isKeyPressed(rtk::Key::Escape)){
                    LOG_DEBUG("Key [Close] pressed.");
                    _inputState.closeRequested = true;
                }
                if (_event.isKeyPressed(rtk::Key::Enter)) {
                    LOG_DEBUG("Key [Confirm] pressed.");
                    _inputState.actions |= rtk::InputAction::Confirm;
                }
            }

            void onStop(rtk::ecs::Registry&) override
            {
            }

        private:
            rtk::Event _event;
            rtk::RenderWindow& _window;

            rtk::InputState& _inputState;
    };
}