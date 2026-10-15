#pragma once

namespace rtype::server {
/**
 * @addtogroup api_server
 * @{
 */

class Game;

/**
 * @brief Identifiers of the server scenes.
 */
enum class ServerSceneType {
  Lobby, ///< Waiting for players.
  InGame ///< Round in progress.
};

/**
 * @brief Input passed to server scenes, empty because inputs come from the
 * network.
 */
struct ServerInput {};

/**
 * @brief Object passed to every server scene constructor.
 */
struct ServerDependency {
  Game &game; ///< Game that owns the scenes.
};

/** @} */
} // namespace rtype::server
