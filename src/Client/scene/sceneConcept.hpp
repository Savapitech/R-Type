#pragma once

#include <concepts>
#include <optional>

#include "GameEngine.hpp"
#include "inputAction/InputAction.hpp"

namespace rtype::client
{
    enum class SceneType
    {
        Menu,
        Game
    };

    template<typename T>
    concept Scene = requires(T& scene, rtk::EngineContext& context, const rtk::InputState& inputState, float dt)
    {
        { scene.onEnter(context) } -> std::same_as<void>;
        { scene.onUpdate(context, inputState, dt) } -> std::same_as<std::optional<SceneType>>;
        { scene.onExit(context) } -> std::same_as<void>;
    };
}