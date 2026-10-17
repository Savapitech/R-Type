#include "Resources.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace rtype::res {

#ifdef RTYPE_EMBED_ASSETS

std::span<const std::uint8_t> read(const std::string &path) {
  std::span<const std::uint8_t> data;

  if (!embeddedLookup(path, data))
    throw std::runtime_error("Unknown embedded resource: " + path);
  return data;
}

std::string materialize(const std::string &path) {
  static std::mutex mutex;
  const auto target = std::filesystem::temp_directory_path() / "rtype" / path;
  const auto data = read(path);

  std::lock_guard lock(mutex);
  if (!std::filesystem::exists(target) ||
      std::filesystem::file_size(target) != data.size()) {
    std::filesystem::create_directories(target.parent_path());
    std::ofstream file(target, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char *>(data.data()),
               static_cast<std::streamsize>(data.size()));
    if (!file)
      throw std::runtime_error("Failed to extract resource: " + path);
  }
  return target.string();
}

#else

std::span<const std::uint8_t> read(const std::string &path) {
  static std::mutex mutex;
  static std::map<std::string, std::vector<std::uint8_t>> cache;

  std::lock_guard lock(mutex);
  auto it = cache.find(path);
  if (it == cache.end()) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
      throw std::runtime_error("Failed to open resource: " + path);
    it = cache
             .emplace(path, std::vector<std::uint8_t>(
                                std::istreambuf_iterator<char>(file), {}))
             .first;
  }
  return it->second;
}

std::string materialize(const std::string &path) {
  return path;
}

#endif

nlohmann::json readJson(const std::string &path) {
  const auto data = read(path);

  return nlohmann::json::parse(data.begin(), data.end());
}

} // namespace rtype::res
