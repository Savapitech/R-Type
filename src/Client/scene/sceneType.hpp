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
  Game        ///< In game.
};

class Game;
struct clientDependency {
  rtk::RenderWindow &window;
  rtype::client::NetworkClient &networkClient;
  Game &game;
};
/** @} */
} // namespace rtype::client
