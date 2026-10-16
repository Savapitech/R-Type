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
/**
 * @addtogroup api_engine
 * @{
 */

/**
 * @brief Owns the ECS world and runs the main loop of a game.
 */
class GameEngine {
public:
  /// Monotonic clock used to measure frame times.
  using Clock = std::chrono::steady_clock;

  GameEngine() = default;
  ~GameEngine() = default;

  /**
   * @brief Returns the entity and component registry.
   * @return The registry.
   */
  ecs::Registry &registry() { return _registry; }

  /**
   * @brief Returns the system scheduler.
   * @return The scheduler.
   */
  ecs::Scheduler &scheduler() { return _scheduler; }

  /**
   * @brief Returns the event bus shared by the systems.
   * @return The event bus.
   */
  busEvent &eventBus() { return _eventBus; }

  /**
   * @brief Asks the main loop to exit after the current frame.
   */
  void stop() { _running = false; }

  /**
   * @brief Sets the frame rate cap.
   * @param fps Frames per second, 0 for no cap.
   */
  void setFPS(uint32_t fps) { _targetFps = fps; }

  /**
   * @brief Loads the game and runs frames until it stops.
   *
   * Each frame calls `onUpdate` on the game, runs every system with the
   * elapsed time clamped to 0.25 s, flushes the registry and then sleeps to
   * respect the frame rate cap.
   *
   * @tparam TGame Type satisfying the rtk::Game concept.
   * @param game Game to run.
   * @return 0 when the loop ends normally.
   */
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
        if constexpr (requires {
                        { game.shouldContinue() } -> std::convertible_to<bool>;
                      }) {
          if (!game.shouldContinue())
            break;
        }

        auto newTime = Clock::now();
        float realDeltaTime =
            std::chrono::duration<float>(newTime - currentTime).count();
        currentTime = newTime;

        float frameTime = std::min(realDeltaTime, 0.25f);
        accumulator += frameTime;

        while (accumulator >= fixedDeltaTime) {
          accumulator -= fixedDeltaTime;
        }

        if constexpr (requires {
                        {
                          game.onUpdate(context, frameTime)
                        } -> std::same_as<void>;
                      }) {
          game.onUpdate(context, frameTime);
        }
        _scheduler.run(_registry, frameTime);
        _registry.flush();

        updateFPS(realDeltaTime);

        if (_targetFps > 0) {
          const auto targetDuration =
              std::chrono::nanoseconds(1000000000 / _targetFps);

          nextFrameTarget += targetDuration;

          auto sleepStart = Clock::now();

          if (nextFrameTarget > sleepStart) {
            auto timeToSleep = nextFrameTarget - sleepStart;

            if (timeToSleep > std::chrono::milliseconds(2))
              std::this_thread::sleep_for(timeToSleep -
                                          std::chrono::milliseconds(2));

            while (Clock::now() < nextFrameTarget) {
            }
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

/** @} */
} // namespace rtk
