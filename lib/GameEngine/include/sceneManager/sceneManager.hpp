#pragma once

#include <concepts>
#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>

#include "sceneConcept.hpp"

namespace rtk
{
    template<SceneIdentifier TSceneId, typename TDependency, typename TInput, typename... TScenes>
    requires ((Scene<TScenes, TSceneId, TInput> && ...) && (std::constructible_from<TScenes, TDependency&> && ...))
    class SceneManager
    {
    public:
        using SceneVariant = std::variant<TScenes...>;

        template<typename TInitialScene>
        SceneManager(TDependency& dependency, std::in_place_type_t<TInitialScene>)
            : _dependency(dependency),
              _currentScene(std::in_place_type<TInitialScene>, dependency)
        {
        }

        void start(EngineContext& context)
        {
            std::visit(
                [&](auto& scene)
                {
                    scene.onEnter(context);
                },
                _currentScene
            );
        }

        void update(EngineContext& context, const TInput& input, float dt)
        {
            const auto requestedScene = std::visit(
                [&](auto& scene)
                {
                    return scene.onUpdate(context, input, dt);
                },
                _currentScene
            );

            if (requestedScene.has_value())
                switchScene(context, *requestedScene);
        }

        void stop(EngineContext& context)
        {
            std::visit(
                [&](auto& scene)
                {
                    scene.onExit(context);
                },
                _currentScene
            );
        }

        template<typename TScene>
        requires (std::same_as<TScene, TScenes> || ...)
        [[nodiscard]] TScene* get() noexcept
        {
            return std::get_if<TScene>(&_currentScene);
        }

        template<typename TScene>
        requires (std::same_as<TScene, TScenes> || ...)
        [[nodiscard]] const TScene* get() const noexcept
        {
            return std::get_if<TScene>(&_currentScene);
        }

    private:
        template<typename TScene>
        bool trySwitchScene(TSceneId nextScene)
        {
            if (nextScene != TScene::Type)
                return false;

            _currentScene.template emplace<TScene>(_dependency);
            return true;
        }

        void switchScene(EngineContext& context, TSceneId nextScene)
        {
            stop(context);

            const bool found = (trySwitchScene<TScenes>(nextScene) || ...);

            if (!found)
                throw std::runtime_error("Unknown scene");

            start(context);
        }

        TDependency& _dependency;
        SceneVariant _currentScene;
    };
}