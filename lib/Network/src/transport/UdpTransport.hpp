#pragma once

/**
 * @addtogroup api_network
 * @{
 */

#include <array>
#include <asio.hpp>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include "../config/NetworkConfig.hpp"
#include "../metrics/Metrics.hpp"

/**
 * @brief Asynchronous UDP socket wrapper used by the server.
 */
class UdpTransport {
public:
  /**
   * @brief Opens an IPv4 UDP socket bound to a port.
   * @param context Asio context that runs the socket handlers.
   * @param port Local port to listen on.
   * @param metrics Counters updated on every send and receive.
   */
  UdpTransport(asio::io_context &context, std::uint16_t port,
               NetworkMetrics &metrics);
  ~UdpTransport() = default;

  /**
   * @brief Starts the receive loop that forwards datagrams to the callback.
   */
  void startReceive();

  /**
   * @brief Queues a datagram for sending, ignored if larger than the limit.
   * @param destination Address and port of the receiver.
   * @param data Bytes to send.
   */
  void send(const asio::ip::udp::endpoint &destination,
            const std::vector<std::uint8_t> &data);

  /**
   * @brief Cancels pending operations and closes the socket.
   */
  void close();

  /**
   * @brief Tells whether the socket is still open.
   * @return True if the socket is open.
   */
  bool isOpen() const;

  /**
   * @brief Sets the function called for every received datagram.
   * @param callback Receives the sender address and the datagram bytes.
   */
  void
  setReceiveCallback(std::function<void(const asio::ip::udp::endpoint &Endpoint,
                                        const std::vector<std::uint8_t> &)>
                         callback);

private:
  static constexpr std::size_t BUFFER_SIZE = 2048;

  asio::ip::udp::socket _socket;
  asio::ip::udp::endpoint _endpoint;
  NetworkMetrics &_metrics;

  std::array<std::uint8_t, BUFFER_SIZE> _receiveBuffer;
  std::function<void(const asio::ip::udp::endpoint &Endpoint,
                     const std::vector<std::uint8_t> &)>
      _receiveCallback;

  void execClose();
};

/** @} */
