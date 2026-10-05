#pragma once

#include <asio/ip/udp.hpp>
#include <cstdint>
#include <asio.hpp>

#include "../protocol/PacketHeader.hpp"
#include "../tracking/PacketTracker.hpp"

class Connection {
  public:

    Connection(std::uint32_t id, asio::ip::udp::endpoint &endpoint);

    std::uint32_t getId() const;
    const asio::ip::udp::endpoint &getEndpoint() const;

    PacketHeader makeHeader();
    PacketReceiveRes onPacketReceived(const PacketHeader &header);
  private:
    std::uint32_t _id;
    asio::ip::udp::endpoint _endpoint;
    PacketTracker _tracker;
};
