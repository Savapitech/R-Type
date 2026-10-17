#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace rtype::res {

/**
 * @brief Rewrite the assts in a temp directory
 *
 * @throws std::runtime_error If the assets cannot be extracted.
 */
void mount();

#ifdef RTYPE_EMBED_ASSETS
struct EmbeddedAsset {
  const char *path;
  const std::uint8_t *data;
  std::size_t size;
};

std::span<const EmbeddedAsset> embeddedAssets();

std::string_view embeddedBuildId();
#endif

} // namespace rtype::res
