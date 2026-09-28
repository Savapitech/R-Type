#include <array>
#include <asio.hpp>
#include <cstddef>
#include <cstdint>
#include <asio.hpp>
#include <functional>
#include <vector>

class UdpTransport {
  public:

    UdpTransport(asio::io_context& context, std::uint16_t port);

    void startReceive();

    void send(const asio::ip::udp::endpoint& destination,
        std::vector<std::uint8_t>& data);

    void setReceiveCallback(std::function<void(const asio::ip::udp::endpoint &Endpoint, const std::vector<std::uint8_t>&)>);

  private:
    static constexpr std::size_t BUFFER_SIZE = 2048;

    asio::ip::udp::socket _socket;
    asio::ip::udp::endpoint _endpoint;

    std::array<std::uint8_t, BUFFER_SIZE> _receiveBuffer;
    std::function<void(const asio::ip::udp::endpoint &Endpoint, const std::vector<std::uint8_t>&)> _receiveCallback;
};
