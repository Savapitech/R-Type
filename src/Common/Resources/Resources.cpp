#include "Resources.hpp"

#ifdef RTYPE_EMBED_ASSETS

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace rtype::res {

void mount() {
  namespace fs = std::filesystem;
  const fs::path root =
      fs::temp_directory_path() / ("rtype-" + std::string(embeddedBuildId()));
  std::error_code error;

  for (const auto &asset : embeddedAssets()) {
    const fs::path target = root / asset.path;

    if (fs::exists(target, error) && fs::file_size(target, error) == asset.size)
      continue;
    fs::create_directories(target.parent_path(), error);

    fs::path partial = target;
    partial += ".part";
    {
      std::ofstream file(partial, std::ios::binary | std::ios::trunc);
      file.write(reinterpret_cast<const char *>(asset.data),
                 static_cast<std::streamsize>(asset.size));
      if (!file)
        throw std::runtime_error("Failed to extract asset: " +
                                 std::string(asset.path));
    }
    fs::rename(partial, target, error);
    if (error)
      throw std::runtime_error("Failed to extract asset: " +
                               std::string(asset.path));
  }
  fs::current_path(root, error);
  if (error)
    throw std::runtime_error("Failed to enter " + root.string());
}

} // namespace rtype::res

#else

namespace rtype::res {

void mount() {}

} // namespace rtype::res

#endif
