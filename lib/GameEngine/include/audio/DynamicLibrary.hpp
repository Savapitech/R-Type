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

  /**
   * @brief Constructs and loads a dynamic library from a file path.
   * @param path Path to the dynamic library.
   */
  explicit DynamicLibrary(const std::string &path) { load(path); }

  ~DynamicLibrary() { unload(); }

  DynamicLibrary(const DynamicLibrary &) = delete;
  DynamicLibrary &operator=(const DynamicLibrary &) = delete;

  /**
   * @brief Move constructor.
   * @param other Dynamic library instance to move from.
   */
  DynamicLibrary(DynamicLibrary &&other) noexcept
      : _handle(other._handle), _lastError(std::move(other._lastError)) {
    other._handle = nullptr;
  }

  /**
   * @brief Move assignment operator.
   * @param other Dynamic library instance to move from.
   * @return Reference to this instance.
   */
  DynamicLibrary &operator=(DynamicLibrary &&other) noexcept {
    if (this != &other) {
      unload();
      _handle = other._handle;
      _lastError = std::move(other._lastError);
      other._handle = nullptr;
    }
    return *this;
  }

  /**
   * @brief Loads a dynamic library from a file path.
   * @param path Path to the shared library.
   * @return True if successfully loaded, false otherwise.
   */
  bool load(const std::string &path) {
    unload();
#if defined(_WIN32)
    _handle = LoadLibraryA(path.c_str());
    if (!_handle) {
      _lastError = "Failed to load library: " + path + " (error " +
                   std::to_string(GetLastError()) + ")";
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

  /**
   * @brief Unloads the currently loaded dynamic library if any.
   */
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

  /**
   * @brief Checks if a dynamic library is currently loaded.
   * @return True if loaded, false otherwise.
   */
  [[nodiscard]] bool isLoaded() const noexcept { return _handle != nullptr; }

  /**
   * @brief Retrieves a function or variable symbol from the loaded library.
   * @tparam T Type to cast the symbol pointer to.
   * @param symbolName Name of the symbol to look up.
   * @return Cast pointer to the symbol, or nullptr if not found or not loaded.
   */
  template <typename T>
  [[nodiscard]] T getSymbol(const std::string &symbolName) const {
    if (!_handle)
      return nullptr;
#if defined(_WIN32)
    FARPROC sym =
        GetProcAddress(static_cast<HMODULE>(_handle), symbolName.c_str());
    return reinterpret_cast<T>(sym);
#else
    void *sym = dlsym(_handle, symbolName.c_str());
    return reinterpret_cast<T>(sym);
#endif
  }

  /**
   * @brief Returns the last error message recorded during library operations.
   * @return Error message string.
   */
  [[nodiscard]] const std::string &getLastError() const noexcept {
    return _lastError;
  }

  /**
   * @brief Returns the platform-specific shared library file extension.
   * @return File extension string (.so, .dylib, or .dll).
   */
  [[nodiscard]] static constexpr const char *getExtension() noexcept {
#if defined(_WIN32)
    return ".dll";
#elif defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
  }

  /**
   * @brief Returns the platform-specific shared library file prefix.
   * @return Prefix string ("lib" on Unix, "" on Windows).
   */
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
