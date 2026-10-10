#include "Server.hpp"

#include <iostream>
#include <utility>

#include "Protocol/PayloadCodec.hpp"
#include "protocol/PacketReader.hpp"

namespace rtype::server {
Server::Server(std::uint16_t port) : _network(port) {
  _network.setOnClientConnected([this](std::uint32_t connectionId) {
    std::cout << "Client connected: " << connectionId << std::endl;
  });

  _network.setOnClientDisconnected([this](std::uint32_t connectionId) {
    std::cout << "Client disconnected: " << connectionId << std::endl;
  });

  _network.setOnMessage([this](std::uint32_t connectionId, MessageType type,
                               const std::vector<std::uint8_t> &payload) {
    handleMessage(connectionId, type, payload);
  });
}

Server::~Server() { stop(); }

void Server::start() { _network.start(); }

void Server::stop() { _network.stop(); }

void Server::send(std::uint32_t connectionId,
                  const std::vector<std::uint8_t> &payload) {
  _network.send(connectionId, payload);
}

void Server::handleMessage(std::uint32_t connectionId, MessageType type,
                           const std::vector<std::uint8_t> &payload) {
  switch (type) {
  case MessageType::Input: {
    PacketReader reader(payload);
    protocol::InputPayload input;

    if (!protocol::readPayload(reader, input) || !reader.isEnd())
      return;
    std::cout << "Input (seq=" << input.inputSequence.get()
              << "): " << static_cast<int>(input.actions) << std::endl;
    break;
  }

  default:
    break;
  }
}
} // namespace rtype::server
