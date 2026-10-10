#include <cstddef>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <optional>
#include <vector>

#include "GameEngine.hpp"
#include "components/Transform.hpp"
#include "systems/InputSystem.hpp"
#include "systems/RenderSystem.hpp"

#include "game.hpp"

int main() {
  try {
    rtype::client::Game game;
    rtk::GameEngine engine;

    return engine.run(game);
  } catch (const std::exception &error) {
    std::cerr << "CLIENT: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}