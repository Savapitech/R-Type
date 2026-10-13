#pragma once
#include <cstdint>
#include <atomic>

namespace rtype::server {

/**
 * @brief singleton NetworkIdGenerator for attribuate networkId
 */
class NetworkIdGenerator {
private:
    std::atomic<std::uint32_t> _nextId{1000}; 

    NetworkIdGenerator() = default;

public:
    static NetworkIdGenerator& getInstance() {
        static NetworkIdGenerator instance;
        return instance;
    }

    NetworkIdGenerator(const NetworkIdGenerator&) = delete;
    NetworkIdGenerator& operator=(const NetworkIdGenerator&) = delete;

    std::uint32_t getNextId() {
        return _nextId++;
    }
};

} // namespace rtype::server