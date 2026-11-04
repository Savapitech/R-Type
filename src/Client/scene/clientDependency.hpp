#pragma once
#include "NetworkClient.hpp"
#include "graphical/renderWindow.hpp"

namespace rtype::client {
class Game;

struct clientDependancy {
  rtk::RenderWindow &window;
  rtype::client::NetworkClient &networkClient;
  Game &game;
};

} // namespace rtype::client