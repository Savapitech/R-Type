#pragma once

/**
 * @file Test.hpp
 * @brief Minimal unit test framework used by every test executable.
 *
 * A test is declared with TEST(Suite, Name) and registers itself before main
 * runs. A failed check throws, so the rest of the test is skipped and the
 * runner goes on with the next test. It has no dependency, so the tests build
 * on every platform the project supports without touching vcpkg.
 */

#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace rtype::test {

/// A registered test.
struct TestCase {
  std::string suite;          ///< Group the test belongs to.
  std::string name;           ///< Name of the test inside its suite.
  std::function<void()> body; ///< Test code.
};

/// Every test of the executable, in declaration order.
inline std::vector<TestCase> &registry() {
  static std::vector<TestCase> tests;
  return tests;
}

/// Adds a test to the registry from a static initializer.
struct Registrar {
  Registrar(const char *suite, const char *name, void (*body)()) {
    registry().push_back({suite, name, body});
  }
};

/// Thrown by a failed check, caught by the runner.
struct Failure : std::exception {
  std::string message; ///< Where and why the check failed.

  explicit Failure(std::string text) : message(std::move(text)) {}
  const char *what() const noexcept override { return message.c_str(); }
};

[[noreturn]] inline void fail(const char *file, int line,
                              const std::string &message) {
  std::ostringstream stream;
  stream << file << ":" << line << ": " << message;
  throw Failure(stream.str());
}

template <typename T, typename = void> struct IsPrintable : std::false_type {};

template <typename T>
struct IsPrintable<T, std::void_t<decltype(std::declval<std::ostream &>()
                                           << std::declval<const T &>())>>
    : std::true_type {};

/// Text shown for a value in a failure message.
template <typename T> std::string describe(const T &value) {
  std::ostringstream stream;
  if constexpr (std::is_enum_v<T>) {
    stream << +static_cast<std::underlying_type_t<T>>(value);
  } else if constexpr (std::is_same_v<T, bool>) {
    stream << (value ? "true" : "false");
  } else if constexpr (std::is_same_v<T, std::uint8_t> ||
                       std::is_same_v<T, std::int8_t> ||
                       std::is_same_v<T, char>) {
    stream << +value;
  } else if constexpr (IsPrintable<T>::value) {
    stream << value;
  } else {
    stream << "<value>";
  }
  return stream.str();
}

template <typename A, typename B>
void checkEqual(const A &actual, const B &expected, const char *actualText,
                const char *expectedText, const char *file, int line) {
  if (actual == expected)
    return;
  fail(file, line,
       std::string("CHECK_EQ(") + actualText + ", " + expectedText +
           ")\n    actual:   " + describe(actual) +
           "\n    expected: " + describe(expected));
}

template <typename A, typename B>
void checkNotEqual(const A &actual, const B &unexpected, const char *actualText,
                   const char *unexpectedText, const char *file, int line) {
  if (!(actual == unexpected))
    return;
  fail(file, line,
       std::string("CHECK_NE(") + actualText + ", " + unexpectedText +
           ")\n    both are: " + describe(actual));
}

inline void checkNear(double actual, double expected, double tolerance,
                      const char *actualText, const char *expectedText,
                      const char *file, int line) {
  if (std::fabs(actual - expected) <= tolerance)
    return;
  fail(file, line,
       std::string("CHECK_NEAR(") + actualText + ", " + expectedText +
           ")\n    actual:   " + describe(actual) + "\n    expected: " +
           describe(expected) + " +- " + describe(tolerance));
}

} // namespace rtype::test

#define RTYPE_TEST_CONCAT_(a, b) a##b
#define RTYPE_TEST_CONCAT(a, b) RTYPE_TEST_CONCAT_(a, b)

/// Declares and registers a test.
#define TEST(suite, name)                                                      \
  static void RTYPE_TEST_CONCAT(test_##suite##_, name)();                      \
  static const ::rtype::test::Registrar RTYPE_TEST_CONCAT(                     \
      registrar_##suite##_, name)(#suite, #name,                               \
                                  &RTYPE_TEST_CONCAT(test_##suite##_, name));  \
  static void RTYPE_TEST_CONCAT(test_##suite##_, name)()

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition))                                                          \
      ::rtype::test::fail(__FILE__, __LINE__, "CHECK(" #condition ")");        \
  } while (false)

#define CHECK_FALSE(condition)                                                 \
  do {                                                                         \
    if (condition)                                                             \
      ::rtype::test::fail(__FILE__, __LINE__, "CHECK_FALSE(" #condition ")");  \
  } while (false)

#define CHECK_EQ(actual, expected)                                             \
  ::rtype::test::checkEqual((actual), (expected), #actual, #expected,          \
                            __FILE__, __LINE__)

#define CHECK_NE(actual, unexpected)                                           \
  ::rtype::test::checkNotEqual((actual), (unexpected), #actual, #unexpected,   \
                               __FILE__, __LINE__)

#define CHECK_NEAR(actual, expected, tolerance)                                \
  ::rtype::test::checkNear(                                                    \
      static_cast<double>(actual), static_cast<double>(expected),              \
      static_cast<double>(tolerance), #actual, #expected, __FILE__, __LINE__)

#define CHECK_THROWS(expression)                                               \
  do {                                                                         \
    bool thrown_ = false;                                                      \
    try {                                                                      \
      static_cast<void>(expression);                                           \
    } catch (const ::rtype::test::Failure &) {                                 \
      throw;                                                                   \
    } catch (...) {                                                            \
      thrown_ = true;                                                          \
    }                                                                          \
    if (!thrown_)                                                              \
      ::rtype::test::fail(__FILE__, __LINE__,                                  \
                          "CHECK_THROWS(" #expression ") did not throw");      \
  } while (false)

#define CHECK_NOTHROW(expression)                                              \
  do {                                                                         \
    try {                                                                      \
      static_cast<void>(expression);                                           \
    } catch (const ::rtype::test::Failure &) {                                 \
      throw;                                                                   \
    } catch (...) {                                                            \
      ::rtype::test::fail(__FILE__, __LINE__,                                  \
                          "CHECK_NOTHROW(" #expression ") threw");             \
    }                                                                          \
  } while (false)
