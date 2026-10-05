#pragma once

#include <variant>
#include <optional>
#include <vector>

#include "sceneConcept.hpp"
#include "menuScene.hpp"
#include "gameScene.hpp"

namespace rtype::client
{
    class SceneManager
    {
    public:
        using SceneVariant = std::variant<MenuScene, GameScene>;

        SceneManager(rtk::RenderWindow& window) : _window(window), _currentScene(std::in_place_type<MenuScene>, window) {}

        void start(rtk::EngineContext& context)
        {
            std::visit(
                [&](Scene auto& scene)
                {
                    scene.onEnter(context);
                },
                _currentScene
            );
        }

        void update(rtk::EngineContext& context, const rtk::InputState& inputState, float dt)
        {
            const auto requestedScene = std::visit(
                [&](Scene auto& scene)
                {
                    return scene.onUpdate(
                        context,
                        inputState,
                        dt
                    );
                },
                _currentScene
            );

            if (requestedScene.has_value())
                switchScene(context, *requestedScene);
        }

        void stop(rtk::EngineContext& context)
        {
            std::visit(
                [&](Scene auto& scene) {
                    scene.onExit(context);
                },
                _currentScene
            );
        }

        template<Scene TScene>
        [[nodiscard]]
        TScene* get()
        {
            return std::get_if<TScene>(&_currentScene);
        }

        template<Scene TScene>
        [[nodiscard]]
        const TScene* get() const
        {
            return std::get_if<TScene>(&_currentScene);
        }

    private:
        void switchScene(rtk::EngineContext& context, SceneType nextScene)
        {
            stop(context);

            switch (nextScene) {
                case SceneType::Menu:
                    _currentScene.emplace<MenuScene>(_window);
                    break;

                case SceneType::Game:
                    _currentScene.emplace<GameScene>(_window);
                    break;
            }

            start(context);
        }

        rtk::RenderWindow& _window;
        SceneVariant _currentScene;
    };
}