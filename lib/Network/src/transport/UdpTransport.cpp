#include <asio.hpp>
#include <asio/error.hpp>
#include <asio/error_code.hpp>
#include <asio/post.hpp>
#include <asio/registered_buffer.hpp>
#include "UdpTransport.hpp"
#include <iostream>

UdpTransport::UdpTransport(asio::io_context &context, std::uint16_t port, NetworkMetrics &metrics)
  : _socket(context, asio::ip::udp::endpoint(asio::ip::udp::v4(), port)), _metrics(metrics) {
}

void UdpTransport::startReceive() {
  _socket.async_receive_from(asio::buffer(_receiveBuffer), _endpoint,
      [this](
        const asio::error_code& error,
        std::size_t bytesReceived
        ) 
      {
        if (error) {
          if (error == asio::error::operation_aborted)
            return;

          std::cerr << error.message() << std::endl;

          if (_socket.is_open()) {
            startReceive();
          }
          return;
        }

        onPacketReceived(_metrics, bytesReceived);

        std::vector<std::uint8_t> data(_receiveBuffer.begin(), _receiveBuffer.begin() + bytesReceived);

        auto sender = _endpoint;

        if (_receiveCallback) {
          _receiveCallback(sender, data);
        }
        if (_socket.is_open()) {
          startReceive();
        }
      }
  );
}

void UdpTransport::send(const asio::ip::udp::endpoint &endpoint, const std::vector<std::uint8_t>& data) {
  auto buffer = std::make_shared<std::vector<std::uint8_t>>(data);

  asio::post(_socket.get_executor(),
      [this, endpoint, buffer] {            
        if (!_socket.is_open())
          return;
        _socket.async_send_to(asio::buffer(*buffer),endpoint,
          [this, buffer](const asio::error_code &error, std::size_t bytesSent) {
            if (error) {
              if (error != asio::error::operation_aborted)
                std::cerr << error.message() << std::endl;

              return;
            }
            onPacketSend(_metrics, bytesSent);
          }
        ); 
      });
}

void UdpTransport::setReceiveCallback(std::function<void(
      const asio::ip::udp::endpoint &Endpoint,
      const std::vector<std::uint8_t>&)> callback) {
  _receiveCallback = std::move(callback);
}

bool UdpTransport::isOpen() const {
  return _socket.is_open();
}

void UdpTransport::execClose() {
  if(!_socket.is_open())
    return;

  asio::error_code error;
  _socket.cancel(error);

  if(error) 
    std::cerr << "Udp cancelling error: " << error.message() << std::endl;
  
  error.clear();
  _socket.close(error);
  
  if(error) 
    std::cerr << "Udp closing error: " << error.message() << std::endl;
}

void UdpTransport::close() {
  asio::post(_socket.get_executor(),
      [this]() {
        execClose();
      }
      );
}
