#pragma once

#include <Logger/Logger.hpp>
#include <algorithm>
#include <chrono>
#include <string>
#include <thread>

#include "EngineContext.hpp"
#include "Game.hpp"
#include "events/busEvents.hpp"

namespace rtk {
class GameEngine {
public:
  using Clock = std::chrono::steady_clock;

  GameEngine() = default;
  ~GameEngine() = default;

  ecs::Registry &registry() { return _registry; }
  ecs::Scheduler &scheduler() { return _scheduler; }
  busEvent &eventBus() { return _eventBus; }

  void stop() { _running = false; }
  void setFPS(uint32_t fps) { _targetFps = fps; }

  template <Game TGame> int run(TGame &game) {
    EngineContext context{
        .registry = _registry,
        .scheduler = _scheduler,
        .eventBus = _eventBus,
    };

    game.onLoad(context);
    _running = true;

    auto currentTime = Clock::now();
    auto nextFrameTarget = currentTime;

    float accumulator = 0.0f;
    const float fixedDeltaTime = 1.0f / 60.0f;

    _scheduler.start(_registry);

    try {
      while (_running) {
        if constexpr (requires { { game.shouldContinue() } -> std::convertible_to<bool>; }) {
          if (!game.shouldContinue()) break;
        }

        auto newTime = Clock::now();
        float realDeltaTime = std::chrono::duration<float>(newTime - currentTime).count();
        currentTime = newTime;

        float frameTime = std::min(realDeltaTime, 0.25f); 
        accumulator += frameTime;

        while (accumulator >= fixedDeltaTime) {
            accumulator -= fixedDeltaTime;
        }

        if constexpr (requires { { game.onUpdate(context, frameTime) } -> std::same_as<void>; }) {
          game.onUpdate(context, frameTime);
        }
        _scheduler.run(_registry, frameTime);
        _registry.flush();

        updateFPS(realDeltaTime);

        if (_targetFps > 0) {
            const auto targetDuration = std::chrono::nanoseconds(1000000000 / _targetFps);

            nextFrameTarget += targetDuration; 

            auto sleepStart = Clock::now();

            if (nextFrameTarget > sleepStart) {
                auto timeToSleep = nextFrameTarget - sleepStart;

                if (timeToSleep > std::chrono::milliseconds(2))
                    std::this_thread::sleep_for(timeToSleep - std::chrono::milliseconds(2));

                while (Clock::now() < nextFrameTarget) {}
            } else {
                nextFrameTarget = Clock::now(); 
            }
        }
      }
    } catch (...) {
      _scheduler.stop(_registry);
      throw;
    }

    _scheduler.stop(_registry);
    if constexpr (requires { game.onUnload(context); }) {
      game.onUnload(context);
    }
    return 0;
  }

private:
  void updateFPS(float realDeltaTime) {
    _fpsTimer += realDeltaTime;
    _fpsFrames++;
    if (_fpsTimer >= 1.0f) {
      LOG_DEBUG("FPS: " + std::to_string(_fpsFrames));
      _fpsTimer -= 1.0f;
      _fpsFrames = 0;
    }
  }

  ecs::Registry _registry;
  ecs::Scheduler _scheduler;
  busEvent _eventBus;

  bool _running = false;
  uint32_t _targetFps = 0;

  float _fpsTimer = 0.0f;
  int _fpsFrames = 0;
};
} // namespace rtk