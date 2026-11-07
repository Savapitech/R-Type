#include "Capture.hpp"
#include <cstdio>
#include <cstdlib>
#include <mutex>

namespace {
std::FILE *openCaptureFile() {
  const char *path = std::getenv("RTYPE_CAPTURE_FILE");
  if (path == nullptr || *path == '\0')
    return nullptr;
  return std::fopen(path, "ab");
}
} // namespace

void captureBody(const std::vector<std::uint8_t> &body) {
  static std::mutex mutex;
  static std::FILE *file = openCaptureFile();
  if (file == nullptr || body.empty() || body.size() > 0xFFFF)
    return;

  std::lock_guard<std::mutex> lock(mutex);
  const std::uint8_t length[2] = {static_cast<std::uint8_t>(body.size() & 0xFF),
                                  static_cast<std::uint8_t>(body.size() >> 8)};
  std::fwrite(length, 1, 2, file);
  std::fwrite(body.data(), 1, body.size(), file);
  std::fflush(file);
}
