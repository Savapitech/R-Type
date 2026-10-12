#pragma once

#include <asio.hpp>
#include <thread>
#include <vector>
#include <array>
#include <string>

#include "events/busEvents.hpp"
#include "events/NetworkEvent.hpp"
#include "protocol/HandshakeHeader.hpp"
#include "protocol/PacketReader.hpp"
#include "Logger/Logger.hpp"

namespace rtype::client {

class NetworkClient {
private:
    asio::io_context _ioContext;
    asio::ip::udp::socket _socket;
    asio::ip::udp::endpoint _serverEndpoint;
    std::array<std::uint8_t, 2048> _recvBuffer;
    std::thread _ioThread;

    busEvent* _eventBus = nullptr;

    bool _isRunning;

    void startReceive() {
        _socket.async_receive_from(
            asio::buffer(_recvBuffer), _serverEndpoint,
            [this](std::error_code ec, std::size_t bytes_recvd) {
                if (!ec && bytes_recvd > 0) {
                    handlePacket(bytes_recvd);
                }
                if (_isRunning) {
                    startReceive();
                }
            });
    }

    void handlePacket(std::size_t size) {
        PacketReader reader(_recvBuffer.data(), size);

        if (size >= HandshakeHeader::HEADER_SIZE) {
            HandshakeHeader header;
            if (header.deserialize(reader)) {
                if (header.magic == MAGIC) {
                    if (_eventBus != nullptr) {
                        if (header.type == HandshakeType::Welcome) {
                            _eventBus->publish(rtype::events::ConnectedToServerEvent{});
                        } else if (header.type == HandshakeType::Rejected) {
                            _eventBus->publish(rtype::events::ConnectionRejectedEvent{});
                        }
                    }
                    return;
                }
            }
        }
    }

public:
    NetworkClient()
        : _socket(_ioContext), _isRunning(false) {}

    ~NetworkClient() {
        stop();
    }

    void setEventBus(busEvent& eventBus) {
        _eventBus = &eventBus;
    }

    bool connect(const std::string& host, const std::string& port) {
        try {
            asio::ip::udp::resolver resolver(_ioContext);
            _serverEndpoint = *resolver.resolve(asio::ip::udp::v4(), host, port).begin();
            _socket.open(asio::ip::udp::v4());

            _isRunning = true;
            startReceive();

            _ioThread = std::thread([this]() { _ioContext.run(); });

            LOG_INFO("NetworkClient: " + host + ":" + port);
            return true;
        } catch (const std::exception& e) {
            LOG_ERROR(std::string("Error Asio: ") + e.what());
            return false;
        }
    }

    void send(const std::vector<std::uint8_t>& data) {
        if (!_socket.is_open()) return;

        _socket.async_send_to(
            asio::buffer(data), _serverEndpoint,
            [](std::error_code ec, std::size_t) {
                if (ec) {
                    LOG_ERROR("Error UDP: " + ec.message());
                }
            });
    }

    void stop() {
        _isRunning = false;
        if (_socket.is_open()) {
            std::error_code ec;
            _socket.cancel(ec);
            _socket.close(ec);
        }
        _ioContext.stop();
        if (_ioThread.joinable()) {
            _ioThread.join();
        }
    }
};

} // namespace rtype::client