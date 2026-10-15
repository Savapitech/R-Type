#pragma once

#include <concepts>
#include <optional>

#include "GameEngine.hpp"

namespace rtk {
/**
 * @addtogroup api_engine
 * @{
 */

/**
 * @brief Requirements for a type used to identify scenes, which must be an
 * enum.
 *
 * @tparam T Candidate identifier type.
 */
template <typename T>
concept SceneIdentifier = std::is_enum_v<T>;

/**
 * @brief Requirements for a scene managed by rtk::SceneManager.
 *
 * A valid scene has a static `Type` identifier and the `onEnter()`,
 * `onUpdate()` and `onExit()` methods. `onUpdate()` returns the identifier
 * of the next scene, or `std::nullopt` to stay on the current one.
 *
 * @tparam T Concrete scene type being validated.
 * @tparam TSceneId Type used to identify scenes, usually an enum.
 * @tparam TInput Type containing the input state passed to the scene.
 *
 * @code
 * static_assert(rtk::Scene<MenuScene, SceneType, rtk::InputState>);
 * @endcode
 */
template <typename T, typename TSceneId, typename TInput>
concept Scene =
    SceneIdentifier<TSceneId> &&
    requires(T &scene, EngineContext &context, const TInput &input, float dt) {
      { T::Type } -> std::convertible_to<TSceneId>;
      { scene.onEnter(context) } -> std::same_as<void>;
      {
        scene.onUpdate(context, input, dt)
      } -> std::same_as<std::optional<TSceneId>>;
      { scene.onExit(context) } -> std::same_as<void>;
    };

/** @} */
} // namespace rtk
