#include <cstddef>
#include <cstdlib>
#include <optional>
#include <exception>
#include <iostream>
#include <vector>

#include "GameEngine.hpp"
#include "components/Transform.hpp"
#include "systems/RenderSystem.hpp"
#include "systems/InputSystem.hpp"

#include "game.hpp"

int main()
{
    try {
        rtype::client::Game game;
        rtk::GameEngine engine;

        return engine.run(game);
    } catch (const std::exception& error) {
        std::cerr << "CLIENT: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}