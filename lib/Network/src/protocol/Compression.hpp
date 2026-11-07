#pragma once

/**
 * @addtogroup api_network
 * @{
 */

#include "../config/NetworkConfig.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

/// zstd compression level used for packet body
inline constexpr int COMPRESSION_LEVEL = 1;

/**
 * @brief Returns the zstd dictionary embedded in the binary.
 *
 * Client and server must use the same bytes: it is part of the protocol. It is
 * embedded at build time from `zstd_dict.bin` by `cmake/EmbedDictionary.cmake`.
 */
std::span<const std::uint8_t> compressionDictionary();

/**
 * @brief Compresses the body of a packet (all its messages).
 *
 * The output is a zstd frame without magic number, checksum, content size or
 * dictionary id, compressed with compressionDictionary(). Contexts are kept per
 * thread, so the function can be called from any thread.
 *
 * @param body Messages to compress.
 * @return The compressed bytes, or nothing if they are not strictly smaller
 * than the input (the body is then sent as it is).
 */
std::optional<std::vector<std::uint8_t>>
compressBody(const std::vector<std::uint8_t> &body);

/**
 * @brief Decompress the body of a packet.
 *
 * The output is capped at MAX_MESSAGE_SIZE bytes, so a small malicious frame
 * cannot expand into a large buffer.
 *
 * @param data Compressed bytes.
 * @param size Number of compressed bytes.
 * @param out Receives the decompressed messages.
 * @return True on success, false if the frame is invalid, empty or would
 * expand to more than MAX_MESSAGE_SIZE bytes.
 */
bool decompressBody(const std::uint8_t *data, std::size_t size,
                    std::vector<std::uint8_t> &out);

/** @} */
