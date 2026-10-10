#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <thread>

#include "Server.hpp"

#define DEFAULT_PORT 4242

int main() {
  try {
    rtype::server::Server server(DEFAULT_PORT);

    server.start();

    std::cout << "Server started on port " << DEFAULT_PORT << std::endl;

    for (;;)
      ;
  } catch (const std::exception &error) {
    std::cerr << error.what() << std::endl;
    return EXIT_FAILURE;
  }
}
