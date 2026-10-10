#pragma once

#include <bit>
#include <vector>
#include <span>

#include <graphical/renderWindow.hpp>

#include "systems/RenderConcept.hpp"

class RtkWindowAdapter {
public:
    RtkWindowAdapter(rtk::RenderWindow& window) : _window(window) {}

    bool pollEvents(rtk::ecs::concepts::Event& engineEvent) {
        rtk::Event rtkEvent;
        bool isOpen = _window.pollEvents(rtkEvent);

        for (int i = 0; i < 256; ++i) {
            engineEvent.keyPressed[i] = rtkEvent.isKeyPressed(static_cast<rtk::Key>(i));
            engineEvent.keyReleased[i] = rtkEvent.isKeyReleased(static_cast<rtk::Key>(i));
        }

        for (int i = 0; i < static_cast<int>(rtk::MouseButton::ButtonCount); ++i) {
            engineEvent.mouseButtonPressed[i] = rtkEvent.isMouseButtonPressed(static_cast<rtk::MouseButton>(i));
            engineEvent.mouseButtonReleased[i] = rtkEvent.isMouseButtonReleased(static_cast<rtk::MouseButton>(i));
        }
        engineEvent.mouseX = rtkEvent.getMouseX();
        engineEvent.mouseY = rtkEvent.getMouseY();

        for (int joy = 0; joy < 4; ++joy) {
            engineEvent.gamepadConnected[joy] = rtkEvent.isGamepadConnected(joy);

            if (engineEvent.gamepadConnected[joy]) {
                for (int b = 0; b < static_cast<int>(rtk::GamepadButton::ButtonCount); ++b) {
                    engineEvent.gamepadButtonPressed[joy][b] = rtkEvent.isGamepadButtonPressed(joy, static_cast<rtk::GamepadButton>(b));
                    engineEvent.gamepadButtonReleased[joy][b] = rtkEvent.isGamepadButtonReleased(joy, static_cast<rtk::GamepadButton>(b));
                }
                for (int a = 0; a < static_cast<int>(rtk::GamepadAxis::AxisCount); ++a) {
                    engineEvent.gamepadAxis[joy][a] = rtkEvent.getGamepadAxis(joy, static_cast<rtk::GamepadAxis>(a));
                }
            } else {
                for (int b = 0; b < static_cast<int>(rtk::GamepadButton::ButtonCount); ++b) {
                    engineEvent.gamepadButtonPressed[joy][b] = false;
                    engineEvent.gamepadButtonReleased[joy][b] = false;
                }
                for (int a = 0; a < static_cast<int>(rtk::GamepadAxis::AxisCount); ++a) {
                    engineEvent.gamepadAxis[joy][a] = 0.0f;
                }
            }
        }

        return isOpen;
    }

    bool beginFrame(rtk::ecs::concepts::ColorRGBA8 color) {
        return _window.beginFrame(rtk::RGB{color.r, color.g, color.b});
    }

    void draw(std::span<const rtk::ecs::concepts::SpriteData> sprites) {
        _rtkSpritesCache.clear();

        if (_rtkSpritesCache.capacity() < sprites.size())
            _rtkSpritesCache.reserve(sprites.size());

        for (const auto& ecsSprite : sprites)
            _rtkSpritesCache.push_back(std::bit_cast<rtk::SpriteData>(ecsSprite));

        _window.draw(_rtkSpritesCache);
    }

    void endFrame() {
        _window.endFrame();
    }

private:
    rtk::RenderWindow& _window;

    std::vector<rtk::SpriteData> _rtkSpritesCache;
};