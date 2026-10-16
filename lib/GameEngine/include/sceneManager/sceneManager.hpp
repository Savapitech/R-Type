#pragma once

#include <concepts>
#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>

#include "sceneConcept.hpp"

namespace rtk {
/**
 * @addtogroup api_engine
 * @{
 */

/**
 * @brief Holds the active scene and switches between a fixed set of scenes.
 *
 * Scenes are stored in a `std::variant`, so the set is checked at compile
 * time and only the active scene is alive.
 *
 * @tparam TSceneId Enum identifying the scenes.
 * @tparam TDependency Object passed to every scene constructor.
 * @tparam TInput Input state passed to `onUpdate()`.
 * @tparam TScenes Every scene type, each satisfying rtk::Scene.
 */
template <SceneIdentifier TSceneId, typename TDependency, typename TInput,
          typename... TScenes>
  requires((Scene<TScenes, TSceneId, TInput> && ...) &&
           (std::constructible_from<TScenes, TDependency &> && ...))
class SceneManager {
public:
  /// Variant holding the active scene.
  using SceneVariant = std::variant<TScenes...>;

  /**
   * @brief Constructs the initial scene without entering it.
   * @tparam TInitialScene First active scene.
   * @param dependency Object passed to every scene constructor.
   */
  template <typename TInitialScene>
  SceneManager(TDependency &dependency, std::in_place_type_t<TInitialScene>)
      : _dependency(dependency),
        _currentScene(std::in_place_type<TInitialScene>, dependency) {}

  /**
   * @brief Calls `onEnter()` on the active scene.
   * @param context Engine services.
   */
  void start(EngineContext &context) {
    std::visit([&](auto &scene) { scene.onEnter(context); }, _currentScene);
  }

  /**
   * @brief Updates the active scene and switches if it asks to.
   * @param context Engine services.
   * @param input Input state of the frame.
   * @param dt Elapsed time since the last frame, in seconds.
   * @throws std::runtime_error If the requested scene is not in TScenes.
   */
  void update(EngineContext &context, const TInput &input, float dt) {
    const auto requestedScene = std::visit(
        [&](auto &scene) { return scene.onUpdate(context, input, dt); },
        _currentScene);

    if (requestedScene.has_value())
      switchScene(context, *requestedScene);
  }

  /**
   * @brief Calls `onExit()` on the active scene.
   * @param context Engine services.
   */
  void stop(EngineContext &context) {
    std::visit([&](auto &scene) { scene.onExit(context); }, _currentScene);
  }

  /**
   * @brief Returns the active scene if it has the given type.
   * @tparam TScene Scene type to look for.
   * @return The scene, or nullptr if another scene is active.
   */
  template <typename TScene>
    requires(std::same_as<TScene, TScenes> || ...)
  [[nodiscard]] TScene *get() noexcept {
    return std::get_if<TScene>(&_currentScene);
  }

  /**
   * @brief Returns the active scene if it has the given type.
   * @tparam TScene Scene type to look for.
   * @return The scene, or nullptr if another scene is active.
   */
  template <typename TScene>
    requires(std::same_as<TScene, TScenes> || ...)
  [[nodiscard]] const TScene *get() const noexcept {
    return std::get_if<TScene>(&_currentScene);
  }

private:
  template <typename TScene> bool trySwitchScene(TSceneId nextScene) {
    if (nextScene != TScene::Type)
      return false;

    _currentScene.template emplace<TScene>(_dependency);
    return true;
  }

  void switchScene(EngineContext &context, TSceneId nextScene) {
    stop(context);

    const bool found = (trySwitchScene<TScenes>(nextScene) || ...);

    if (!found)
      throw std::runtime_error("Unknown scene");

    start(context);
  }

  TDependency &_dependency;
  SceneVariant _currentScene;
};

/** @} */
} // namespace rtk
