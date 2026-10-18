#include <cstddef>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>

#include "GameEngine.hpp"
#include "Resources/Resources.hpp"
#include "game.hpp"

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT "4242"

static void printUsage(std::string_view progName) {
  std::cout << "Usage: " << progName << " [-h host] [-p port] [--help]\n"
            << "Options:\n"
            << "  -h, --host <address>  Server address to connect to (default: "
            << DEFAULT_HOST << ")\n"
            << "  -p, --port <port>     Server port to connect to (default: "
            << DEFAULT_PORT << ")\n"
            << "  --help                Display this help message\n";
}

int main(int argc, char *argv[]) {
  try {
    rtype::res::mount();
    std::string host = DEFAULT_HOST;
    std::string port = DEFAULT_PORT;

    for (int i = 1; i < argc; ++i) {
      std::string_view arg = argv[i];
      if (arg == "--help") {
        printUsage(argv[0]);
        return EXIT_SUCCESS;
      }
      if (arg == "-h" || arg == "--host") {
        if (i + 1 < argc && argv[i + 1][0] != '-') {
          host = argv[++i];
        } else if (argc == 2 && arg == "-h") {
          printUsage(argv[0]);
          return EXIT_SUCCESS;
        } else {
          std::cerr << "Error: " << arg << " requires a host address\n";
          printUsage(argv[0]);
          return EXIT_FAILURE;
        }
      } else if (arg == "-p" || arg == "--port") {
        if (i + 1 < argc) {
          port = argv[++i];
        } else {
          std::cerr << "Error: " << arg << " requires a port number\n";
          printUsage(argv[0]);
          return EXIT_FAILURE;
        }
      } else {
        std::cerr << "Unknown option: " << arg << "\n";
        printUsage(argv[0]);
        return EXIT_FAILURE;
      }
    }

    rtype::client::Game game(host, port);
    rtk::GameEngine engine;

    engine.setFPS(60);

    return engine.run(game);
  } catch (const std::exception &error) {
    std::cerr << "CLIENT: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}