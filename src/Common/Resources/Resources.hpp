#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace rtype::res {

/**
 * @brief Reads a resource, ex: "assets/shoot.wav".
 *
 * Dev builds read the file from disk (relative to the working directory),
 * embedded builds read it from the binary. The returned bytes stay valid for
 * the whole program.
 * @throws std::runtime_error If the resource does not exist.
 */
std::span<const std::uint8_t> read(const std::string &path);

/**
 * @brief Reads a resource and parses it as JSON.
 * @throws std::runtime_error If the resource does not exist.
 * @throws nlohmann::json::parse_error If it is not valid JSON.
 */
nlohmann::json readJson(const std::string &path);

/**
 * @brief Gives a real file path for libraries that can only open files.
 *
 * Dev builds return @p path untouched, embedded builds extract the resource
 * once into the temp directory and return that path.
 */
std::string materialize(const std::string &path);

/**
 * @brief Reads the blueprint at @p path and registers it in @p factory.
 * @return The archetype id of the blueprint.
 */
template <typename Factory>
auto loadBlueprint(Factory &factory, const std::string &name,
                   const std::string &path) {
  return factory.registerBlueprint(name, readJson(path));
}

#ifdef RTYPE_EMBED_ASSETS
/** @brief Implemented by the generated EmbeddedAssets.cpp. */
bool embeddedLookup(std::string_view path, std::span<const std::uint8_t> &out);
#endif

} // namespace rtype::res
