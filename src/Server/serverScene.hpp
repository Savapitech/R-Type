#pragma once

namespace rtype::server {
class Game;

enum class ServerSceneType { Lobby, InGame };

struct ServerInput {};

struct ServerDependency {
  Game &game;
};
} // namespace rtype::server