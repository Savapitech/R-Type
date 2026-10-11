#pragma once
#include <functional>
#include <stdexcept>
#include <ISystem.hpp>
#include "inputAction/inputMapping.hpp"
#include "systems/RenderConcept.hpp"

namespace rtk::systems {

template <ecs::concepts::IsRenderWindow WindowType>
class InputSystem final : public rtk::ecs::ISystem {
public:
  InputSystem(std::reference_wrapper<WindowType> window,
              rtk::InputState &inputState,
              const rtk::Input::InputMapping& mapping)
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

    const auto& bindings = _mapping.getBindings();

    for (uint8_t i = 0; i < 32; ++i) {
        ecs::concepts::Key boundKey = bindings[i];

        if (boundKey != ecs::concepts::Key::Unknown)
            if (_event.isKeyPressed(boundKey))
                _inputState.actionMask |= (1 << i);
    }

    if (_event.isKeyPressed(ecs::concepts::Key::Escape)) {
        _inputState.closeRequested = true;
    }

    if (_event.isKeyPressed(ecs::concepts::Key::F3) && _event.isKeyReleased(ecs::concepts::Key::B)) {
        _inputState.debug = !_inputState.debug;
    }
  }

private:
  ecs::concepts::Event _event;
  WindowType &_window;
  rtk::InputState &_inputState;
  const rtk::Input::InputMapping& _mapping;
};

} // namespace rtk::systems