#include "Server.hpp"

#include <iostream>
#include <utility>

#include "Protocol/PayloadCodec.hpp"
#include "protocol/PacketReader.hpp"

namespace rtype::server {

static bool isSequenceNewer(std::uint32_t sequence, std::uint32_t previous) {
    return static_cast<std::int32_t>(sequence - previous) > 0;
}

Server::Server(std::uint16_t port) : _network(port) {
  _network.setOnClientConnected([this](std::uint32_t connectionId) {
    std::cout << "Client connected: " << connectionId << std::endl;
  });

  _network.setOnClientDisconnected([this](std::uint32_t connectionId) {
    _lastInputSequence.erase(connectionId);
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
    if (!protocol::readPayload(reader, input) || !reader.isEnd()) {
        return;
    }
    const std::uint32_t sequence = input.inputSequence.get();
    auto it = _lastInputSequence.find(connectionId);

    if (it != _lastInputSequence.end()) {
        if (!isSequenceNewer(sequence, it->second)) {
            return;
        }
        it->second = sequence;
    } else {
        _lastInputSequence.emplace(connectionId, sequence);
    }

    {
        std::lock_guard<std::mutex> lock(_inputMutex);

        _inputQueue.push({connectionId, input});
    }
    break;
}

  default:
    break;
  }
}

bool Server::pollInput(InputEvent &event) {
    std::lock_guard<std::mutex> lock(_inputMutex);

    if (_inputQueue.empty())
        return false;

    event = _inputQueue.front();
    _inputQueue.pop();
    return true;
}
} // namespace rtype::server
