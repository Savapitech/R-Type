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

  void setFPS(uint32_t fps) {
      _targetFps = fps;
  }

  template <Game TGame> int run(TGame &game) {
    EngineContext context{
        .registry = _registry,
        .scheduler = _scheduler,
        .eventBus = _eventBus,
    };

    game.onLoad(context);

    _running = true;
    _previousTime = Clock::now();

    _scheduler.start(_registry);

    try {
      while (_running) {
        if constexpr (requires {
                        { game.shouldContinue() } -> std::convertible_to<bool>;
                      }) {
          if (!game.shouldContinue()) {
            break;
          }
        }
        const float dt = computeDeltaTime();

        if constexpr (requires {
                        { game.onUpdate(context, dt) } -> std::same_as<void>;
                      }) {
          game.onUpdate(context, dt);
        }

        _scheduler.run(_registry, dt);

        if (_targetFps > 0) {
            const auto targetDuration = std::chrono::nanoseconds(1000000000 / _targetFps);
            const auto nextFrameTime = _previousTime + targetDuration;
            std::this_thread::sleep_until(nextFrameTime);
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
  float computeDeltaTime() {
    const auto currentTime = Clock::now();

    const float dt =
        std::chrono::duration<float>(currentTime - _previousTime).count();

    _previousTime = currentTime;

    _fpsTimer += dt;
    _fpsFrames++;
    if (_fpsTimer >= 1.0f) {
      LOG_DEBUG("FPS: " + std::to_string(_fpsFrames));
      _fpsTimer -= 1.0f;
      _fpsFrames = 0;
    }

    return std::min(dt, 0.1f);
  }

  ecs::Registry _registry;
  ecs::Scheduler _scheduler;
  busEvent _eventBus;

  Clock::time_point _previousTime;

  bool _running = false;

  uint32_t _targetFps = 0;

  float _fpsTimer = 0.0f;
  int _fpsFrames = 0;
};
} // namespace rtk