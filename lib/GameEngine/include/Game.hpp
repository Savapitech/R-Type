#pragma once

#include <concepts>

namespace rtk
{
    struct EngineContext;

    template<typename T>
    concept Game = requires(T& game, EngineContext& context) {
        { game.onLoad(context) } -> std::same_as<void>;
    };
}