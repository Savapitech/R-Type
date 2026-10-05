#pragma once

#include <stdexcept>
#include <vector>

#include <ISystem.hpp>
#include <sprite/spriteData.hpp>
#include <graphical/renderWindow.hpp>

#include "components/Transform.hpp"
#include "InputAction.hpp"


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
                _inputState.actions = InputAction::None;
                _window.pollEvents(_event);

                if (_event.isKeyPressed(rtk::Key::Up))
                    _inputState.actions |= InputAction::Up;
                if (_event.isKeyPressed(rtk::Key::Down))
                    _inputState.actions |= InputAction::Down;
                if (_event.isKeyPressed(rtk::Key::Left))
                    _inputState.actions |= InputAction::Left;
                if (_event.isKeyPressed(rtk::Key::Right))
                    _inputState.actions |= InputAction::Right;
                if (_event.isKeyPressed(rtk::Key::Space))
                    _inputState.actions |= InputAction::Shoot;
                if (_event.isKeyPressed(rtk::Key::Escape))
                    _inputState.closeRequested = true;

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