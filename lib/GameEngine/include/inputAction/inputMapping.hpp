#pragma once

#include "systems/RenderConcept.hpp"
#include <array>
#include <cstdint>

namespace rtk::Input {
/**
 * @addtogroup api_engine
 * @{
 */

/**
 * @brief Gamepad axis binding with its activation threshold.
 */
struct AxisData {
  ecs::concepts::GamepadAxis axis; ///< Bound axis.
  float validationPression; ///< Threshold, negative values trigger below it.
};

/**
 * @brief One physical control bound to an action.
 */
struct InputBinding {
  /**
   * @brief Kind of physical control.
   */
  enum class Type : uint8_t {
    NoneAction,    ///< Empty slot.
    Keyboard,      ///< Keyboard key.
    GamepadButton, ///< Gamepad button.
    GamepadAxis,   ///< Gamepad axis past a threshold.
    Mouse          ///< Mouse button.
  };

  Type type{Type::NoneAction}; ///< Which union member is active.

  union {
    ecs::concepts::Key key;              ///< Bound key.
    ecs::concepts::GamepadButton button; ///< Bound gamepad button.
    ecs::concepts::MouseButton mouse;    ///< Bound mouse button.
    AxisData axisData;                   ///< Bound gamepad axis.
  };

  /**
   * @brief Creates an empty binding.
   */
  InputBinding() : type(Type::NoneAction) {}
};

/**
 * @brief Table binding up to four physical controls to each of 32 actions.
 */
class InputMapping {
public:
  /// Number of actions, one per bit of InputState::actionMask.
  static constexpr uint8_t MAX_ACTIONS = 32;
  /// Number of controls that can trigger the same action.
  static constexpr uint8_t BINDINGS_PER_ACTION = 4;

private:
  std::array<InputBinding, MAX_ACTIONS * BINDINGS_PER_ACTION> _bindings;

public:
  /**
   * @brief Binds a keyboard key to an action.
   * @param bitIndex Action index, from 0 to 31.
   * @param key Key to bind.
   */
  void bindKey(uint8_t bitIndex, ecs::concepts::Key key) {
    addBinding(bitIndex, [key](InputBinding &b) {
      b.type = InputBinding::Type::Keyboard;
      b.key = key;
    });
  }

  /**
   * @brief Binds a gamepad button to an action.
   * @param bitIndex Action index, from 0 to 31.
   * @param button Button to bind.
   */
  void bindGamepad(uint8_t bitIndex, ecs::concepts::GamepadButton button) {
    addBinding(bitIndex, [button](InputBinding &b) {
      b.type = InputBinding::Type::GamepadButton;
      b.button = button;
    });
  }

  /**
   * @brief Binds a mouse button to an action.
   * @param bitIndex Action index, from 0 to 31.
   * @param mouseButton Button to bind.
   */
  void bindMouse(uint8_t bitIndex, ecs::concepts::MouseButton mouseButton) {
    addBinding(bitIndex, [mouseButton](InputBinding &b) {
      b.type = InputBinding::Type::Mouse;
      b.mouse = mouseButton;
    });
  }

  /**
   * @brief Binds a gamepad axis to an action.
   * @param bitIndex Action index, from 0 to 31.
   * @param axis Axis to bind.
   * @param validationPression Threshold, negative values trigger below it.
   */
  void bindGamepadAxis(uint8_t bitIndex, ecs::concepts::GamepadAxis axis,
                       float validationPression) {
    addBinding(bitIndex, [axis, validationPression](InputBinding &b) {
      b.type = InputBinding::Type::GamepadAxis;
      b.axisData.axis = axis;
      b.axisData.validationPression = validationPression;
    });
  }

  /**
   * @brief Returns every binding slot.
   * @return Slots of action n are at indices 4n to 4n + 3.
   */
  const std::array<InputBinding, MAX_ACTIONS * BINDINGS_PER_ACTION> &
  getBindings() const {
    return _bindings;
  }

private:
  template <typename Func> void addBinding(uint8_t bitIndex, Func assignFunc) {
    if (bitIndex >= MAX_ACTIONS)
      return;

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

/** @} */
} // namespace rtk::Input
