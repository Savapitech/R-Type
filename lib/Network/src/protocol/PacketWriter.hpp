#pragma once

/**
 * @addtogroup api_network
 * @{
 */

#include "../config/NetworkConfig.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

/**
 * @brief Growable buffer of big-endian values, capped at MAX_PAYLOAD_SIZE.
 */
class PacketWriter {
public:
  /**
   * @brief Creates an empty writer with the full capacity reserved.
   */
  PacketWriter();

  /**
   * @brief Appends one byte.
   * @param value Byte to append.
   * @return True on success, false if the buffer is full.
   */
  bool writeUInt8(std::uint8_t value);

  /**
   * @brief Appends a 16-bit unsigned integer in big-endian order.
   * @param value Value to append.
   * @return True on success, false if the buffer is full.
   */
  bool writeUInt16(std::uint16_t value);

  /**
   * @brief Appends a 32-bit unsigned integer in big-endian order.
   * @param value Value to append.
   * @return True on success, false if the buffer is full.
   */
  bool writeUInt32(std::uint32_t value);

  /**
   * @brief Appends raw bytes.
   * @param data Bytes to append.
   * @param size Number of bytes to append.
   * @return True on success, false if the buffer would overflow.
   */
  bool writeBytes(const std::uint8_t *data, std::size_t size);

  /**
   * @brief Tells whether nothing has been written yet.
   * @return True if the buffer is empty.
   */
  bool empty() const;

  /**
   * @brief Discards every written byte.
   */
  void clear();

  /**
   * @brief Returns the written bytes.
   * @return The underlying buffer.
   */
  const std::vector<std::uint8_t> &getData() const;

  /**
   * @brief Returns the number of written bytes.
   * @return The buffer size.
   */
  std::size_t getSize() const;

private:
  bool canWrite(std::size_t size) const;
  std::vector<std::uint8_t> _buffer;
};

/** @} */
