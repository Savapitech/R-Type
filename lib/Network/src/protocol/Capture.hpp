#pragma once

/**
 * @addtogroup api_network
 * @{
 */

#include <cstdint>
#include <vector>

/**
 * @brief Debug helper that records the body of every connected packet sent.
 *
 * Enabled only when the environment variable `RTYPE_CAPTURE_FILE` is set to a
 * file path. Each record is a 2 byte little-endian length followed by the
 * packet body (messages only, without the PacketHeader). The file is the
 * corpus read by `tools/compression_bench`.
 */
void captureBody(const std::vector<std::uint8_t> &body);

/** @} */
