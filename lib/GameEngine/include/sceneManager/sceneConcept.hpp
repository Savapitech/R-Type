#pragma once

#include <concepts>
#include <optional>

#include "GameEngine.hpp"

namespace rtk
{

    /**
    * @brief define the Scene Concept who let user use the
    * SceneManger bring by the rtk engine
    *
    * A valid Scene contain :
    * - a static `Type` ;
    * - a method `onEnter()` ;
    * - a method `onUpdate()` ;
    * - a method `onExit()`.
    *
    * @tparam TScene Concrete scene type being validated.
    * @tparam TSceneId Type used to identify scenes, usually an enum.
    * @tparam TInput Type containing the input state passed to the scene.
    *
    * @note You can do static asset and template your scene implementation
    * for verifying that your implementation is correct
    *
    * @code
    * static_assert(rtk::Scene<MenuScene, SceneType, rtk::InputState>);
    * @endcode
    */
    template<typename T>
    concept SceneIdentifier = std::is_enum_v<T>;

    template<typename T, typename TSceneId, typename TInput>
    concept Scene =  SceneIdentifier<TSceneId> && requires(T& scene, EngineContext& context, const TInput& input, float dt)
    {
        { T::Type } -> std::convertible_to<TSceneId>;
        { scene.onEnter(context) } -> std::same_as<void>;
        { scene.onUpdate(context, input, dt) } -> std::same_as<std::optional<TSceneId>>;
        { scene.onExit(context) } -> std::same_as<void>;
    };
}