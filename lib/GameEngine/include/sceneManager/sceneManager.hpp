#pragma once

#include <variant>
#include <optional>
#include <vector>

#include "sceneConcept.hpp"

namespace rtk
{
    template<typename SceneId, typename SceneDependency, typename... TScenes>
    requires (Scene<TScenes> && ...)
    class SceneManager
    {
    public:
        using SceneVariant = std::variant<TScenes...>;

        template<typename TInitialScene>
        SceneManager(SceneDependency& dependency, std::in_place_type_t<TInitialScene>)
            : _dependency(dependency),
              _currentScene(std::in_place_type<TInitialScene>, dependency)
        {
        }

        void start(rtk::EngineContext& context)
        {
            std::visit([&](auto& scene) { scene.onEnter(context); }, _currentScene);
        }

        void update(rtk::EngineContext& context, const rtk::InputState& inputState, float dt)
        {
            const auto requestedScene = std::visit(
                [&](auto& scene)
                {
                    return scene.onUpdate(context, inputState, dt);
                },
                _currentScene
            );

            if (requestedScene.has_value())
                switchScene(context, *requestedScene);
        }

        void stop(rtk::EngineContext& context)
        {
            std::visit([&](auto& scene) { scene.onExit(context); }, _currentScene);
        }

    private:
        void switchScene(rtk::EngineContext& context, SceneId nextScene)
        {
            stop(context);

            const bool found = ((nextScene == TScenes::Type ? (
                _currentScene .template emplace<TScenes>(_dependency), true) : false) || ...);

            if (!found)
                throw std::runtime_error("Unknown scene");

            start(context);
        }

        SceneDependency& _dependency;
        SceneVariant _currentScene;
    };
}