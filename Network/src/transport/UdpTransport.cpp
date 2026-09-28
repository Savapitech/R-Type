#include "UdpTransport.hpp"
#include <asio.hpp>
#include <asio/registered_buffer.hpp>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <system_error>
#include <vector>

UdpTransport::UdpTransport(asio::io_context &context, std::uint16_t port)
  : _socket(context, asio::ip::udp::endpoint(asio::ip::udp::v4(), port)) {
}

void UdpTransport::startReceive() {
  _socket.async_receive_from(asio::buffer(_receiveBuffer), _endpoint,
      [this](
        const std::error_code& error,
        std::size_t bytesReceived
        ) 
      {
        if (error) {
          std::cerr << error.message() << std::endl;
          return;
        }

        std::vector<std::uint8_t> data(_receiveBuffer.begin(), _receiveBuffer.begin() + bytesReceived);

        if (_receiveCallback) {
          _receiveCallback(_endpoint, data);
        }

        startReceive();
      }
      );
}

void UdpTransport::send(const asio::ip::udp::endpoint &endpoint, std::vector<std::uint8_t>& data) {
  _socket.send_to(asio::buffer(data.data(), data.size()), endpoint); //to make async later and handle life of the buffer
}

void UdpTransport::setReceiveCallback(std::function<void(
      const asio::ip::udp::endpoint &Endpoint,
      const std::vector<std::uint8_t>&)> callback) {
  _receiveCallback = std::move(callback);
}
