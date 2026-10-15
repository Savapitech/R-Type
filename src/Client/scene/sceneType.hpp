#pragma once

namespace rtype::client {
/**
 * @addtogroup api_client
 * @{
 */

/**
 * @brief Identifiers of the client scenes.
 */
enum class SceneType {
  Menu,       ///< Title screen.
  Connection, ///< Handshake with the server.
  Game        ///< In game.
};

/** @} */
} // namespace rtype::client
