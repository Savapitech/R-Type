#pragma once
#include "NetworkClient.hpp"
#include "graphical/renderWindow.hpp"

namespace rtype::client {
class Game;

/** @brief Dependencies provided to scenes belonging to client. */
struct clientDependancy {
  /** @brief Reference of the window of client. */
  rtk::RenderWindow &window;
  /** @brief Reference of the networkClient. */
  rtype::client::NetworkClient &networkClient;
  /** @brief Reference of the game. */
  Game &game;
};

} // namespace rtype::client