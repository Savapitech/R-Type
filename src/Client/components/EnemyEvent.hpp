#pragma once

#include <cstdint>

namespace rtype::events {
    struct EnemyEscapedEvent {
        std::size_t entityId;
    };
}