#pragma once

/**
 * @addtogroup api_network
 * @{
 */

#include <cstddef>
#include <cstdint>
#include <vector>

/**
 * @brief Bounds checked reader of big-endian values over a byte buffer.
 */
class PacketReader {
public:
  /**
   * @brief Creates a reader over a raw buffer that it does not own.
   * @param data First byte of the buffer.
   * @param size Number of readable bytes.
   */
  PacketReader(const std::uint8_t *data, std::size_t size);

  /**
   * @brief Creates a reader over a vector that must outlive the reader.
   * @param data Buffer to read from.
   */
  PacketReader(const std::vector<uint8_t> &data);

  /**
   * @brief Reads one byte.
   * @param value Receives the byte.
   * @return True on success, false if the buffer is exhausted.
   */
  bool readUInt8(std::uint8_t &value);

  /**
   * @brief Reads a big-endian 16-bit unsigned integer.
   * @param value Receives the decoded value.
   * @return True on success, false if fewer than 2 bytes remain.
   */
  bool readUInt16(std::uint16_t &value);

  /**
   * @brief Reads a big-endian 32-bit unsigned integer.
   * @param value Receives the decoded value.
   * @return True on success, false if fewer than 4 bytes remain.
   */
  bool readUInt32(std::uint32_t &value);

  /**
   * @brief Copies raw bytes out of the buffer.
   * @param destination Buffer of at least `size` bytes.
   * @param size Number of bytes to copy.
   * @return True on success, false if not enough bytes remain.
   */
  bool readBytes(std::uint8_t *destination, std::size_t size);

  /**
   * @brief Returns the number of bytes already read.
   * @return The current read offset.
   */
  std::size_t getOffset() const;

  /**
   * @brief Returns the number of bytes left to read.
   * @return The remaining byte count.
   */
  std::size_t getRemaining() const;

  /**
   * @brief Tells whether every byte has been read.
   * @return True if nothing is left to read.
   */
  bool isEnd() const;

  /**
   * @brief Moves the read offset back to the first byte.
   */
  void reset();

private:
  bool canRead(std::size_t size) const;

  const std::uint8_t *_data;
  std::size_t _size;
  std::size_t _offset;
};

/** @} */
