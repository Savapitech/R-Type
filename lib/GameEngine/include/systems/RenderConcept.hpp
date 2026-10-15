#pragma once

#include <bitset>
#include <concepts>
#include <cstdint>
#include <span>

#include "../utils/vec2.hpp"

/**
 * @brief Backend independent types and concepts used by engine systems.
 */
namespace rtk::ecs::concepts {
/**
 * @addtogroup api_engine
 * @{
 */

/**
 * @brief Keyboard keys, mirroring the values of the graphical library.
 */
enum class Key : uint16_t {
  Unknown = 0,
  A,
  B,
  C,
  D,
  E,
  F,
  G,
  H,
  I,
  J,
  K,
  L,
  M,
  N,
  O,
  P,
  Q,
  R,
  S,
  T,
  U,
  V,
  W,
  X,
  Y,
  Z,
  Num0,
  Num1,
  Num2,
  Num3,
  Num4,
  Num5,
  Num6,
  Num7,
  Num8,
  Num9,
  Escape,
  LControl,
  LShift,
  LAlt,
  LSystem,
  RControl,
  RShift,
  RAlt,
  RSystem,
  Menu,
  LBracket,
  RBracket,
  Semicolon,
  Comma,
  Period,
  Quote,
  Slash,
  Backslash,
  Tilde,
  Equal,
  Hyphen,
  Space,
  Enter,
  Backspace,
  Tab,
  PageUp,
  PageDown,
  End,
  Home,
  Insert,
  Delete,
  Add,
  Subtract,
  Multiply,
  Divide,
  Left,
  Right,
  Up,
  Down,
  Numpad0,
  Numpad1,
  Numpad2,
  Numpad3,
  Numpad4,
  Numpad5,
  Numpad6,
  Numpad7,
  Numpad8,
  Numpad9,
  F1,
  F2,
  F3,
  F4,
  F5,
  F6,
  F7,
  F8,
  F9,
  F10,
  F11,
  F12,
  F13,
  F14,
  F15,
  KeyCount = 256
};

/**
 * @brief Mouse buttons.
 */
enum class MouseButton : uint16_t {
  Left = 0,
  Right = 1,
  Middle = 2,
  ButtonCount = 3
};

/**
 * @brief Gamepad buttons, with PlayStation style aliases.
 */
enum class GamepadButton : uint16_t {
  Unknown = 0,
  A,
  B,
  X,
  Y,
  Cross = A,
  Circle = B,
  Square = X,
  Triangle = Y,
  L1,
  R1,
  L2,
  R2,
  Select,
  Start,
  L3,
  R3,
  DpadUp,
  DpadDown,
  DpadLeft,
  DpadRight,
  Home,
  ButtonCount
};

/**
 * @brief Gamepad analog axes.
 */
enum class GamepadAxis : uint16_t {
  LeftX = 0,
  LeftY,
  RightX,
  RightY,
  L2,
  R2,
  AxisCount
};

/**
 * @brief Input query interface filled by the window backend.
 *
 * The backend stores a context pointer and plain function pointers, so the
 * engine can query inputs without depending on the backend types. Every query
 * returns a neutral value when its function pointer is not set.
 */
class Event {
public:
  /// Backend context passed to every function.
  void *userData{nullptr};
  /// Backend key press query.
  bool (*_isKeyPressed)(void *, Key){nullptr};
  /// Backend key release query.
  bool (*_isKeyReleased)(void *, Key){nullptr};
  /// Backend mouse press query.
  bool (*_isMouseButtonPressed)(void *, MouseButton){nullptr};
  /// Backend mouse release query.
  bool (*_isMouseButtonReleased)(void *, MouseButton){nullptr};
  /// Backend mouse x query.
  int (*_getMouseX)(void *){nullptr};
  /// Backend mouse y query.
  int (*_getMouseY)(void *){nullptr};
  /// Backend gamepad press query.
  bool (*_isGamepadButtonPressed)(void *, int, GamepadButton){nullptr};
  /// Backend gamepad release query.
  bool (*_isGamepadButtonReleased)(void *, int, GamepadButton){nullptr};
  /// Backend gamepad axis query.
  float (*_getGamepadAxis)(void *, int, GamepadAxis){nullptr};
  /// Backend gamepad presence query.
  bool (*_isGamepadConnected)(void *, int){nullptr};

  /**
   * @brief Tells whether a key is held.
   * @param key Key to test.
   * @return True while the key is down.
   */
  [[nodiscard]] bool isKeyPressed(Key key) const {
    return _isKeyPressed ? _isKeyPressed(userData, key) : false;
  }

  /**
   * @brief Tells whether a key was released this frame.
   * @param key Key to test.
   * @return True on the frame the key goes up.
   */
  [[nodiscard]] bool isKeyReleased(Key key) const {
    return _isKeyReleased ? _isKeyReleased(userData, key) : false;
  }

  /**
   * @brief Tells whether a mouse button is held.
   * @param button Button to test.
   * @return True while the button is down.
   */
  [[nodiscard]] bool isMouseButtonPressed(MouseButton button) const {
    return _isMouseButtonPressed ? _isMouseButtonPressed(userData, button)
                                 : false;
  }

  /**
   * @brief Tells whether a mouse button was released this frame.
   * @param button Button to test.
   * @return True on the frame the button goes up.
   */
  [[nodiscard]] bool isMouseButtonReleased(MouseButton button) const {
    return _isMouseButtonReleased ? _isMouseButtonReleased(userData, button)
                                  : false;
  }

  /**
   * @brief Returns the cursor x position.
   * @return X in window pixels.
   */
  [[nodiscard]] int getMouseX() const {
    return _getMouseX ? _getMouseX(userData) : 0;
  }

  /**
   * @brief Returns the cursor y position.
   * @return Y in window pixels.
   */
  [[nodiscard]] int getMouseY() const {
    return _getMouseY ? _getMouseY(userData) : 0;
  }

  /**
   * @brief Tells whether a gamepad button is held.
   * @param joystickId Gamepad index.
   * @param button Button to test.
   * @return True while the button is down.
   */
  [[nodiscard]] bool isGamepadButtonPressed(int joystickId,
                                            GamepadButton button) const {
    return _isGamepadButtonPressed
               ? _isGamepadButtonPressed(userData, joystickId, button)
               : false;
  }

  /**
   * @brief Tells whether a gamepad button was released this frame.
   * @param joystickId Gamepad index.
   * @param button Button to test.
   * @return True on the frame the button goes up.
   */
  [[nodiscard]] bool isGamepadButtonReleased(int joystickId,
                                             GamepadButton button) const {
    return _isGamepadButtonReleased
               ? _isGamepadButtonReleased(userData, joystickId, button)
               : false;
  }

  /**
   * @brief Returns the position of a gamepad axis.
   * @param joystickId Gamepad index.
   * @param axis Axis to read.
   * @return A value between -1 and 1.
   */
  [[nodiscard]] float getGamepadAxis(int joystickId, GamepadAxis axis) const {
    return _getGamepadAxis ? _getGamepadAxis(userData, joystickId, axis) : 0.0f;
  }

  /**
   * @brief Tells whether a gamepad is plugged in.
   * @param joystickId Gamepad index.
   * @return True if the gamepad is connected.
   */
  [[nodiscard]] bool isGamepadConnected(int joystickId) const {
    return _isGamepadConnected ? _isGamepadConnected(userData, joystickId)
                               : false;
  }
};

/**
 * @brief Area of a texture, in texels.
 */
struct TextureRectU16 {
  uint16_t x;      ///< Left edge.
  uint16_t y;      ///< Top edge.
  uint16_t width;  ///< Width.
  uint16_t height; ///< Height.
};

/**
 * @brief 8-bit per channel color.
 */
struct ColorRGBA8 {
  uint8_t r; ///< Red channel.
  uint8_t g; ///< Green channel.
  uint8_t b; ///< Blue channel.
  uint8_t a; ///< Alpha channel.
};

/**
 * @brief Everything the renderer needs to draw one sprite.
 *
 * The layout matches the sprite type of the graphical library so that both
 * can be converted with `std::bit_cast`.
 */
struct alignas(64) SpriteData {
  utils::vec2 position;        ///< Screen position, in pixels.
  utils::vec2 scale{1.f, 1.f}; ///< Scale factor on each axis.
  utils::vec2 size;            ///< Size before scaling, in pixels.
  utils::vec2 origin;          ///< Pivot point, relative to the sprite.

  TextureRectU16 textureRect{}; ///< Area of the texture to draw.
  ColorRGBA8 color{};           ///< Color multiplied with the texture.
  float rotation{0.f};          ///< Rotation in radians.

  uint32_t textureId{0}; ///< Texture handle returned by the window.
  int32_t layer{0};      ///< Draw order, higher layers are drawn on top.
  uint32_t flags{0};     ///< Bit 0 flips horizontally, bit 1 vertically.
  uint32_t reserved{0};  ///< Unused, keeps the layout aligned.
};

/**
 * @brief Requirements for the window backend used by the render and input
 * systems.
 *
 * The backend fills an Event in `pollEvents()`, which returns false once the
 * window is closed, and draws a batch of sprites between `beginFrame()` and
 * `endFrame()`.
 *
 * @tparam T Candidate window backend.
 */
template <typename T>
concept IsRenderWindow = requires(T window, Event &event, ColorRGBA8 color,
                                  std::span<const SpriteData> sprites) {
  { window.pollEvents(event) } -> std::same_as<bool>;
  { window.beginFrame(color) } -> std::same_as<bool>;
  { window.draw(sprites) } -> std::same_as<void>;
  { window.endFrame() } -> std::same_as<void>;
};

/** @} */
} // namespace rtk::ecs::concepts
