#pragma once
#include "inputAction/inputMapping.hpp"
#include "systems/RenderConcept.hpp"
#include <ISystem.hpp>
#include <functional>
#include <stdexcept>

namespace rtk::systems {

template <ecs::concepts::IsRenderWindow WindowType>
class InputSystem final : public rtk::ecs::ISystem {
public:
  InputSystem(std::reference_wrapper<WindowType> window,
              rtk::InputState &inputState,
              const rtk::Input::InputMapping &mapping)
      : _window(window.get()), _inputState(inputState), _mapping(mapping) {};

  ~InputSystem() override = default;

  void onStart(rtk::ecs::Registry &) override {
    LOG_DEBUG("Load Input System");
  }

  void onStop(rtk::ecs::Registry &) override {}

  void update([[maybe_unused]] rtk::ecs::Registry &reg, float) override {
    _inputState.actionMask = 0;
    _inputState.closeRequested = false;

    if (!_window.pollEvents(_event))
      _inputState.closeRequested = true;

    const auto &bindings = _mapping.getBindings();

    for (uint8_t bit = 0; bit < rtk::Input::InputMapping::MAX_ACTIONS; ++bit) {

      size_t startIndex = bit * rtk::Input::InputMapping::BINDINGS_PER_ACTION;

      for (size_t slot = 0;
           slot < rtk::Input::InputMapping::BINDINGS_PER_ACTION; ++slot) {
        const auto &b = bindings[startIndex + slot];

        if (b.type == rtk::Input::InputBinding::Type::NoneAction)
          continue;

        bool isPressed = false;

        if (b.type == rtk::Input::InputBinding::Type::Keyboard)
          isPressed = _event.isKeyPressed(b.key);
        else if (b.type == rtk::Input::InputBinding::Type::GamepadButton)
          isPressed = _event.isGamepadButtonPressed(0, b.button);
        else if (b.type == rtk::Input::InputBinding::Type::Mouse)
          isPressed = _event.isMouseButtonPressed(b.mouse);
        else if (b.type == rtk::Input::InputBinding::Type::GamepadAxis) {
          float pression = _event.getGamepadAxis(0, b.axisData.axis);
          isPressed = (b.axisData.validationPression < 0)
                          ? (pression <= b.axisData.validationPression)
                          : (pression >= b.axisData.validationPression);
        }

        if (isPressed) {
          _inputState.actionMask |= (1 << bit);
          break;
        }
      }
    }

    if (_event.isKeyPressed(ecs::concepts::Key::Escape))
      _inputState.closeRequested = true;

    if (_event.isKeyPressed(ecs::concepts::Key::F3) &&
        _event.isKeyReleased(ecs::concepts::Key::B))
      _inputState.debug = !_inputState.debug;
  }

private:
  ecs::concepts::Event _event;
  WindowType &_window;
  rtk::InputState &_inputState;
  const rtk::Input::InputMapping &_mapping;
};

} // namespace rtk::systems