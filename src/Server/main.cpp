#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>

#include "GameEngine.hpp"
#include "Resources/Resources.hpp"
#include "game.hpp"

#define DEFAULT_PORT 4242

static void printUsage(std::string_view progName) {
  std::cout << "Usage: " << progName << " [-p port] [-h]\n"
            << "Options:\n"
            << "  -p, --port <port>   Port to listen on (default: "
            << DEFAULT_PORT << ")\n"
            << "  -h, --help          Display this help message\n";
}

int main(int argc, char *argv[]) {
  try {
    rtype::res::mount();
    std::uint16_t port = DEFAULT_PORT;

    for (int i = 1; i < argc; ++i) {
      std::string_view arg = argv[i];
      if (arg == "-h" || arg == "--help") {
        printUsage(argv[0]);
        return EXIT_SUCCESS;
      }
      if (arg == "-p" || arg == "--port") {
        if (i + 1 < argc) {
          port = static_cast<std::uint16_t>(std::stoi(argv[++i]));
        } else {
          std::cerr << "Error: " << arg << " requires a port number\n";
          printUsage(argv[0]);
          return EXIT_FAILURE;
        }
      } else if (arg.rfind("-", 0) != 0) {
        port = static_cast<std::uint16_t>(std::stoi(argv[i]));
      } else {
        std::cerr << "Unknown option: " << arg << "\n";
        printUsage(argv[0]);
        return EXIT_FAILURE;
      }
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
