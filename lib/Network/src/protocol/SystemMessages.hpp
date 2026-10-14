#pragma once

#include "MessageHeader.hpp"

namespace systemMessage {
inline constexpr MessageId Ping = 0x03;
inline constexpr MessageId Disconnect = 0x04;
inline constexpr MessageId Pong = 0x86;
} // namespace systemMessage
