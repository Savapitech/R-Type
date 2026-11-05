#pragma once

#include <cstdint>

namespace pong {

/**
 * @brief Identifiers of scenes in Pong.
 */
enum class PongSceneType : std::uint8_t { Menu = 0, Game = 1 };

} // namespace pong
