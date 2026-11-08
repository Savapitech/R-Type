#pragma once

#include <string_view>

namespace rtype::protocol {

/**
 * @brief Tells whether a username is valid: 1 to 16 characters among a-z,
 * A-Z, 0-9 and _.
 * @param name Name to check.
 * @return True if the name can be used.
 */
constexpr bool isValidUsername(std::string_view name) {
  if (name.empty() || name.size() > 16)
    return false;
  for (const char c : name) {
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_'))
      return false;
  }
  return true;
}

} // namespace rtype::protocol
