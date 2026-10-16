#pragma once

#include <concepts>

namespace rtk {
/**
 * @addtogroup api_engine
 * @{
 */

struct EngineContext;

/**
 * @brief Requirements for a type that GameEngine::run() can drive.
 *
 * `onLoad(EngineContext &)` is required. `onUpdate(EngineContext &, float)`,
 * `shouldContinue()` and `onUnload(EngineContext &)` are called when present.
 *
 * @tparam T Candidate game type.
 */
template <typename T>
concept Game = requires(T &game, EngineContext &context) {
  { game.onLoad(context) } -> std::same_as<void>;
};

/** @} */
} // namespace rtk
