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
        bool isOpen = _window.pollEvents(_rtkEventCache);

        engineEvent.userData = this;

        engineEvent._isKeyPressed = [](void* ctx, rtk::ecs::concepts::Key k) {
            return static_cast<RtkWindowAdapter*>(ctx)->_rtkEventCache.isKeyPressed(static_cast<rtk::Key>(k));
        };
        engineEvent._isKeyReleased = [](void* ctx, rtk::ecs::concepts::Key k) {
            return static_cast<RtkWindowAdapter*>(ctx)->_rtkEventCache.isKeyReleased(static_cast<rtk::Key>(k));
        };
        engineEvent._isMouseButtonPressed = [](void* ctx, rtk::ecs::concepts::MouseButton b) {
            return static_cast<RtkWindowAdapter*>(ctx)->_rtkEventCache.isMouseButtonPressed(static_cast<rtk::MouseButton>(b));
        };
        engineEvent._isMouseButtonReleased = [](void* ctx, rtk::ecs::concepts::MouseButton b) {
            return static_cast<RtkWindowAdapter*>(ctx)->_rtkEventCache.isMouseButtonReleased(static_cast<rtk::MouseButton>(b));
        };
        engineEvent._getMouseX = [](void* ctx) {
            return static_cast<RtkWindowAdapter*>(ctx)->_rtkEventCache.getMouseX();
        };
        engineEvent._getMouseY = [](void* ctx) {
            return static_cast<RtkWindowAdapter*>(ctx)->_rtkEventCache.getMouseY();
        };
        engineEvent._isGamepadButtonPressed = [](void* ctx, int j, rtk::ecs::concepts::GamepadButton b) {
            return static_cast<RtkWindowAdapter*>(ctx)->_rtkEventCache.isGamepadButtonPressed(j, static_cast<rtk::GamepadButton>(b));
        };
        engineEvent._isGamepadButtonReleased = [](void* ctx, int j, rtk::ecs::concepts::GamepadButton b) {
            return static_cast<RtkWindowAdapter*>(ctx)->_rtkEventCache.isGamepadButtonReleased(j, static_cast<rtk::GamepadButton>(b));
        };
        engineEvent._getGamepadAxis = [](void* ctx, int j, rtk::ecs::concepts::GamepadAxis a) {
            return static_cast<RtkWindowAdapter*>(ctx)->_rtkEventCache.getGamepadAxis(j, static_cast<rtk::GamepadAxis>(a));
        };
        engineEvent._isGamepadConnected = [](void* ctx, int j) {
            return static_cast<RtkWindowAdapter*>(ctx)->_rtkEventCache.isGamepadConnected(j);
        };

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
    rtk::Event _rtkEventCache;

    std::vector<rtk::SpriteData> _rtkSpritesCache;
};