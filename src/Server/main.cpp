#include <cstdlib>
#include <exception>
#include <iostream>

#include "GameEngine.hpp"
#include "Resources/Resources.hpp"
#include "game.hpp"

#define DEFAULT_PORT 4242

int main(int argc, char *argv[]) {
  try {
    rtype::res::mount();
    std::uint16_t port = DEFAULT_PORT;
    if (argc > 1) {
      port = static_cast<std::uint16_t>(std::stoi(argv[1]));
    }
    rtype::server::Game game(port);
    rtk::GameEngine engine;

    engine.setFPS(60);

    return engine.run(game);
  } catch (const std::exception &error) {
    std::cerr << "SERVER: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
