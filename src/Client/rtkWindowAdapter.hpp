#pragma once

/**
 * @addtogroup api_client
 * @{
 */

#include <bit>
#include <span>
#include <vector>

#include <graphical/renderWindow.hpp>

#include "systems/RenderConcept.hpp"

/**
 * @brief Adapts the window of the graphical library to the IsRenderWindow
 * concept.
 */
class RtkWindowAdapter {
public:
  /**
   * @brief Wraps a window.
   * @param window Window to adapt, which must outlive the adapter.
   */
  RtkWindowAdapter(rtk::RenderWindow &window) : _window(window) {}

  /**
   * @brief Polls window events and points the engine Event at them.
   * @param engineEvent Event filled with the backend queries.
   * @return False once the window is closed.
   */
  bool pollEvents(rtk::ecs::concepts::Event &engineEvent) {
    bool isOpen = _window.pollEvents(_rtkEventCache);

    engineEvent.userData = this;

    engineEvent._isKeyPressed = [](void *ctx, rtk::ecs::concepts::Key k) {
      return static_cast<RtkWindowAdapter *>(ctx)->_rtkEventCache.isKeyPressed(
          static_cast<rtk::Key>(k));
    };
    engineEvent._isKeyReleased = [](void *ctx, rtk::ecs::concepts::Key k) {
      return static_cast<RtkWindowAdapter *>(ctx)->_rtkEventCache.isKeyReleased(
          static_cast<rtk::Key>(k));
    };
    engineEvent._isMouseButtonPressed = [](void *ctx,
                                           rtk::ecs::concepts::MouseButton b) {
      return static_cast<RtkWindowAdapter *>(ctx)
          ->_rtkEventCache.isMouseButtonPressed(
              static_cast<rtk::MouseButton>(b));
    };
    engineEvent._isMouseButtonReleased = [](void *ctx,
                                            rtk::ecs::concepts::MouseButton b) {
      return static_cast<RtkWindowAdapter *>(ctx)
          ->_rtkEventCache.isMouseButtonReleased(
              static_cast<rtk::MouseButton>(b));
    };
    engineEvent._getMouseX = [](void *ctx) {
      return static_cast<RtkWindowAdapter *>(ctx)->_rtkEventCache.getMouseX();
    };
    engineEvent._getMouseY = [](void *ctx) {
      return static_cast<RtkWindowAdapter *>(ctx)->_rtkEventCache.getMouseY();
    };
    engineEvent._isGamepadButtonPressed =
        [](void *ctx, int j, rtk::ecs::concepts::GamepadButton b) {
          return static_cast<RtkWindowAdapter *>(ctx)
              ->_rtkEventCache.isGamepadButtonPressed(
                  j, static_cast<rtk::GamepadButton>(b));
        };
    engineEvent._isGamepadButtonReleased =
        [](void *ctx, int j, rtk::ecs::concepts::GamepadButton b) {
          return static_cast<RtkWindowAdapter *>(ctx)
              ->_rtkEventCache.isGamepadButtonReleased(
                  j, static_cast<rtk::GamepadButton>(b));
        };
    engineEvent._getGamepadAxis = [](void *ctx, int j,
                                     rtk::ecs::concepts::GamepadAxis a) {
      return static_cast<RtkWindowAdapter *>(ctx)
          ->_rtkEventCache.getGamepadAxis(j, static_cast<rtk::GamepadAxis>(a));
    };
    engineEvent._isGamepadConnected = [](void *ctx, int j) {
      return static_cast<RtkWindowAdapter *>(ctx)
          ->_rtkEventCache.isGamepadConnected(j);
    };

    return isOpen;
  }

  /**
   * @brief Starts a frame cleared with a color.
   * @param color Clear color, alpha is ignored.
   * @return False if the frame cannot be drawn.
   */
  bool beginFrame(rtk::ecs::concepts::ColorRGBA8 color) {
    return _window.beginFrame(rtk::RGB{color.r, color.g, color.b});
  }

  /**
   * @brief Draws a batch of sprites.
   * @param sprites Sprites to draw.
   */
  void draw(std::span<const rtk::ecs::concepts::SpriteData> sprites) {
    _rtkSpritesCache.clear();

    if (_rtkSpritesCache.capacity() < sprites.size())
      _rtkSpritesCache.reserve(sprites.size());

    for (const auto &ecsSprite : sprites)
      _rtkSpritesCache.push_back(std::bit_cast<rtk::SpriteData>(ecsSprite));

    _window.draw(_rtkSpritesCache);
  }

  /**
   * @brief Presents the frame.
   */
  void endFrame() { _window.endFrame(); }

private:
  rtk::RenderWindow &_window;
  rtk::Event _rtkEventCache;

  std::vector<rtk::SpriteData> _rtkSpritesCache;
};

/** @} */
