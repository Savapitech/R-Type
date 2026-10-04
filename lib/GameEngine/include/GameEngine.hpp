#pragma once

#include <chrono>
#include <algorithm>

#include "EngineContext.hpp"

namespace rtk
{
    class GameEngine
    {
    public:
        using Clock = std::chrono::steady_clock;

        GameEngine() = default;
        ~GameEngine() = default;


        ecs::Registry& registry()
        {
            return _registry;
        }

        ecs::Scheduler& scheduler()
        {
            return _scheduler;
        }

        void stop()
        {
            _running = false;
        }

        template<Game TGame>
        int run(TGame& game)
        {
            EngineContext context{
                .registry = _registry,
                .scheduler = _scheduler
            };

            game.onLoad(context);

            _running = true;
            _previousTime = Clock::now();

            _scheduler.start(_registry);

            try {
                while (_running) {
                    if constexpr (requires { { game.shouldContinue() } -> std::convertible_to<bool>;}) {
                        if (!game.shouldContinue()) {
                            break;
                        }
                    }
                    const float dt = computeDeltaTime();

                    if constexpr (requires { {game.onUpdate(context, dt)}-> std::same_as<void>;}) {
                        game.onUpdate(context, dt);
                    }

                    _scheduler.run(_registry, dt);
                }
            } catch (...) {
                _scheduler.stop(_registry);
                throw;
            }

            _scheduler.stop(_registry);

            if constexpr (requires {
                game.onUnload(context);
            }) {
                game.onUnload(context);
            }

            return 0;
        }

    private:
        float computeDeltaTime()
        {
            const auto currentTime = Clock::now();

            const float dt = std::chrono::duration<float>(currentTime - _previousTime).count();

            _previousTime = currentTime;

            return std::min(dt, 0.1f);
        }

        ecs::Registry _registry;
        ecs::Scheduler _scheduler;

        Clock::time_point _previousTime;

        bool _running = false;
    };
}