#include "Test.hpp"

#include <cstring>
#include <exception>
#include <iostream>
#include <string>

/**
 * @file Main.cpp
 * @brief Runner shared by the test executables.
 *
 * Usage: `test_x [--list] [filter]`. Without argument every test runs, a
 * filter keeps the tests whose `Suite.Name` contains it.
 */
int main(int argc, char **argv) {
  bool list = false;
  std::string filter;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--list") == 0)
      list = true;
    else
      filter = argv[i];
  }

  const auto &tests = rtype::test::registry();
  std::size_t passed = 0;
  std::size_t failed = 0;

  for (const auto &test : tests) {
    const std::string fullName = test.suite + "." + test.name;
    if (!filter.empty() && fullName.find(filter) == std::string::npos)
      continue;
    if (list) {
      std::cout << fullName << '\n';
      continue;
    }

    try {
      test.body();
      ++passed;
      std::cout << "[  OK  ] " << fullName << '\n';
    } catch (const rtype::test::Failure &failure) {
      ++failed;
      std::cout << "[ FAIL ] " << fullName << '\n'
                << "    " << failure.message << '\n';
    } catch (const std::exception &error) {
      ++failed;
      std::cout << "[ FAIL ] " << fullName << '\n'
                << "    unexpected exception: " << error.what() << '\n';
    } catch (...) {
      ++failed;
      std::cout << "[ FAIL ] " << fullName << '\n'
                << "    unexpected exception\n";
    }
  }

  if (list)
    return 0;
  std::cout << '\n'
            << passed << " passed, " << failed << " failed, " << passed + failed
            << " total" << std::endl;
  return failed == 0 ? 0 : 1;
}
