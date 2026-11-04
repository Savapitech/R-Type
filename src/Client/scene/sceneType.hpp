#pragma once

#include "NetworkClient.hpp"
#include <graphical/renderWindow.hpp>
namespace rtype::client {
/**
 * @addtogroup api_client
 * @{
 */

/**
 * @brief Identifiers of the client scenes.
 */
enum class SceneType {
  Intro,      ///< Intro of the game
  Menu,       ///< Title screen.
  Connection, ///< Handshake with the server.
  RoomList,   ///< List of all the rooms available.
  RoomLobby,      ///< Lobby of a room before the game start.
  Game ///< In game.
};
/** @} */
} // namespace rtype::client
