#pragma once

#include "Room/RoomManager.hpp"
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

class GameInstance;
class Server;

/** @brief Dependencies provided to scenes belonging to a game instance. */
struct GameInstanceDependency {
    /** @brief Instance owning the scene and its ECS world. */
    GameInstance& instance;
    /** @brief Shared UDP server used for network messages. */
    Server& server;
    /** @brief Manager owning the rooms and player ready states. */
    RoomManager &roomManager;
};

/** @} */
} // namespace rtype::server

