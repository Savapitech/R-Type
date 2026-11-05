#pragma once

#include <iostream>
#include <string>
#include <utility>

#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
#else
  #include <dlfcn.h>
#endif

namespace rtk::audio {

/**
 * @brief Cross-platform loader for dynamic libraries (.so / .dylib / .dll).
 */
class DynamicLibrary {
public:
  DynamicLibrary() = default;

  explicit DynamicLibrary(const std::string &path) {
    load(path);
  }

  ~DynamicLibrary() {
    unload();
  }

  DynamicLibrary(const DynamicLibrary &) = delete;
  DynamicLibrary &operator=(const DynamicLibrary &) = delete;

  DynamicLibrary(DynamicLibrary &&other) noexcept
      : _handle(other._handle), _lastError(std::move(other._lastError)) {
    other._handle = nullptr;
  }

  DynamicLibrary &operator=(DynamicLibrary &&other) noexcept {
    if (this != &other) {
      unload();
      _handle = other._handle;
      _lastError = std::move(other._lastError);
      other._handle = nullptr;
    }
    return *this;
  }

  bool load(const std::string &path) {
    unload();
#if defined(_WIN32)
    _handle = LoadLibraryA(path.c_str());
    if (!_handle) {
      _lastError = "Failed to load library: " + path + " (error " + std::to_string(GetLastError()) + ")";
      return false;
    }
#else
    _handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!_handle) {
      const char *err = dlerror();
      _lastError = err ? err : "Unknown dlopen error";
      return false;
    }
#endif
    _lastError.clear();
    return true;
  }

  void unload() {
    if (_handle) {
#if defined(_WIN32)
      FreeLibrary(static_cast<HMODULE>(_handle));
#else
      dlclose(_handle);
#endif
      _handle = nullptr;
    }
  }

  [[nodiscard]] bool isLoaded() const noexcept {
    return _handle != nullptr;
  }

  template <typename T>
  [[nodiscard]] T getSymbol(const std::string &symbolName) const {
    if (!_handle)
      return nullptr;
#if defined(_WIN32)
    FARPROC sym = GetProcAddress(static_cast<HMODULE>(_handle), symbolName.c_str());
    return reinterpret_cast<T>(sym);
#else
    void *sym = dlsym(_handle, symbolName.c_str());
    return reinterpret_cast<T>(sym);
#endif
  }

  [[nodiscard]] const std::string &getLastError() const noexcept {
    return _lastError;
  }

  [[nodiscard]] static constexpr const char *getExtension() noexcept {
#if defined(_WIN32)
    return ".dll";
#elif defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
  }

  [[nodiscard]] static constexpr const char *getPrefix() noexcept {
#if defined(_WIN32)
    return "";
#else
    return "lib";
#endif
  }

private:
  void *_handle = nullptr;
  std::string _lastError{};
};

} // namespace rtk::audio
