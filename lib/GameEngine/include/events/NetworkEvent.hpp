#pragma once

#include <cstdint>

namespace rtype::events {

    struct ConnectedToServerEvent {
        std::uint32_t clientId = 0;
        std::uint32_t playerNetworkId = 0;
        std::uint32_t serverTick = 0;
    };

    struct ConnectionRejectedEvent {
        std::uint8_t reason = 0;
    };

    struct SendPacketEvent {
        std::vector<std::uint8_t> data;
    };

    struct CreateEntityEvent {
        std::uint32_t serverTick;
        std::uint32_t networkId;
        std::uint16_t entityType;
        float positionX;
        float positionY;
        float rotation;
    };

} // namespace rtype::events