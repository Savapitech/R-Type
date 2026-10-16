#pragma once

#include <atomic>
#include <cstdint>

namespace rtype::server {
/**
 * @addtogroup api_gameplay
 * @{
 */

/**
 * @brief Process wide source of unique network entity ids, starting at 1000.
 */
class NetworkIdGenerator {
private:
  std::atomic<std::uint32_t> _nextId{1000};

  NetworkIdGenerator() = default;

public:
  /**
   * @brief Returns the single generator instance.
   * @return The shared generator.
   */
  static NetworkIdGenerator &getInstance() {
    static NetworkIdGenerator instance;
    return instance;
  }

  NetworkIdGenerator(const NetworkIdGenerator &) = delete;
  NetworkIdGenerator &operator=(const NetworkIdGenerator &) = delete;

  /**
   * @brief Returns a new id, thread safe.
   * @return An id never returned before.
   */
  std::uint32_t getNextId() { return _nextId++; }
};

/** @} */
} // namespace rtype::server
