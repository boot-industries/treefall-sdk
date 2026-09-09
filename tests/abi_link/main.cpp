// SPDX-License-Identifier: MIT
#include "orpheus/abi.h"
#include "treefall/abi.h"
#include "treefall/errors.h"

#include <array>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__)
#include <dlfcn.h>
#else
#include <dlfcn.h>
#endif

#ifndef ORP_BUILD_SHARED_CORE
#define ORP_BUILD_SHARED_CORE 0
#endif

namespace fs = std::filesystem;

namespace {

#if defined(_WIN32)
using ModuleHandle = HMODULE;
constexpr std::string_view kSharedExtension = ".dll";

ModuleHandle LoadModule(const fs::path& path) {
  return ::LoadLibraryW(path.wstring().c_str());
}

std::string WideToUtf8(std::wstring_view wide) {
  if (wide.empty()) {
    return {};
  }
  const int required = ::WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                                             nullptr, 0, nullptr, nullptr);
  if (required <= 0) {
    return {};
  }
  std::string utf8(static_cast<std::size_t>(required), '\0');
  const int written = ::WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                                            utf8.data(), required, nullptr, nullptr);
  if (written <= 0) {
    return {};
  }
  utf8.resize(static_cast<std::size_t>(written));
  return utf8;
}

void* LoadSymbol(ModuleHandle handle, const char* name) {
  return reinterpret_cast<void*>(::GetProcAddress(handle, name));
}

void CloseModule(ModuleHandle handle) {
  if (handle != nullptr) {
    ::FreeLibrary(handle);
  }
}

std::string LastErrorString() {
  const DWORD error = ::GetLastError();
  if (error == 0) {
    return "unknown";
  }
  LPWSTR buffer = nullptr;
  const DWORD length = ::FormatMessageW(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&buffer),
      0, nullptr);
  std::string message = "unknown";
  if (length != 0 && buffer != nullptr) {
    std::wstring_view wide(buffer, length);
    while (!wide.empty() && (wide.back() == L'\r' || wide.back() == L'\n' || wide.back() == L' ')) {
      wide.remove_suffix(1);
    }
    const std::string converted = WideToUtf8(wide);
    if (!converted.empty()) {
      message = converted;
    }
  }
  if (buffer != nullptr) {
    ::LocalFree(buffer);
  }
  return message;
}
#else
using ModuleHandle = void*;

ModuleHandle LoadModule(const fs::path& path) {
  const std::string encoded = path.string();
  return dlopen(encoded.c_str(), RTLD_NOW);
}

void* LoadSymbol(ModuleHandle handle, const char* name) {
  return dlsym(handle, name);
}

void CloseModule(ModuleHandle handle) {
  if (handle != nullptr) {
    dlclose(handle);
  }
}

std::string LastErrorString() {
  const char* message = dlerror();
  return message != nullptr ? std::string(message) : std::string("unknown");
}

#if defined(__APPLE__)
constexpr std::string_view kSharedExtension = ".dylib";
#else
constexpr std::string_view kSharedExtension = ".so";
#endif
#endif

struct ModuleInfo {
  const char* library_name;
  const char* legacy_symbol;
  const char* treefall_symbol;
};

void PrintResolution(const std::string& symbol, const void* address) {
  if (address == nullptr) {
    throw std::runtime_error("Symbol returned null pointer for " + symbol);
  }
  std::cout << "Resolved " << symbol << " -> " << address << std::endl;
}
void LoaderLogger(orpheus_log_level, const char*, void*) {}
void LoaderTelemetry(const char*, const char*, void*) {}

} // namespace

int main() {
#if !ORP_BUILD_SHARED_CORE
  std::cout << "abi_link: skipping (shared core disabled)" << std::endl;
  return 0;
#endif

  const fs::path library_dir(ORPHEUS_ABI_LINK_DIR);
  std::cout << "Loading Orpheus ABI libraries from " << library_dir << std::endl;
  const std::array<ModuleInfo, 3> modules{{
      {ORPHEUS_SESSION_LIB, "orpheus_session_abi_v1", "treefall_session_abi_v1"},
      {ORPHEUS_CLIPGRID_LIB, "orpheus_clipgrid_abi_v1", "treefall_clipgrid_abi_v1"},
      {ORPHEUS_RENDER_LIB, "orpheus_render_abi_v1", "treefall_render_abi_v1"},
  }};
  std::vector<ModuleHandle> handles;
  handles.reserve(modules.size());

  try {
    const auto check_factory = [](void* address, const char* name) {
      PrintResolution(name, address);
      uint32_t got_major = 0;
      uint32_t got_minor = 0;
      const auto reject_wrong_major = [&](auto fn) {
        if (fn(ORPHEUS_ABI_MAJOR, &got_major, &got_minor) == nullptr)
          throw std::runtime_error(std::string(name) + " rejected the current ABI");
        if (fn(ORPHEUS_ABI_MAJOR + 1, &got_major, &got_minor) != nullptr ||
            (ORPHEUS_ABI_MAJOR > 0 && fn(ORPHEUS_ABI_MAJOR - 1, &got_major, &got_minor) != nullptr))
          throw std::runtime_error(std::string(name) + " accepted a wrong ABI major");
      };
      if (std::string_view(name).find("session") != std::string_view::npos) {
        reject_wrong_major(
            reinterpret_cast<const orpheus_session_api_v1* (*)(uint32_t, uint32_t*, uint32_t*)>(
                address));
      } else if (std::string_view(name).find("clipgrid") != std::string_view::npos) {
        reject_wrong_major(
            reinterpret_cast<const orpheus_clipgrid_api_v1* (*)(uint32_t, uint32_t*, uint32_t*)>(
                address));
      } else {
        reject_wrong_major(
            reinterpret_cast<const orpheus_render_api_v1* (*)(uint32_t, uint32_t*, uint32_t*)>(
                address));
      }
    };

    ModuleHandle first = nullptr;
    for (const auto& module : modules) {
      const fs::path library_path = library_dir / module.library_name;
      if (library_path.extension() != kSharedExtension)
        throw std::runtime_error("Unexpected shared library extension for " +
                                 library_path.string());
      ModuleHandle handle = LoadModule(library_path);
      if (handle == nullptr)
        throw std::runtime_error("Failed to load " + library_path.string() + ": " +
                                 LastErrorString());
      handles.push_back(handle);
      if (first == nullptr)
        first = handle;
      void* legacy = LoadSymbol(handle, module.legacy_symbol);
      void* treefall = LoadSymbol(handle, module.treefall_symbol);
      check_factory(legacy, module.legacy_symbol);
      check_factory(treefall, module.treefall_symbol);
      uint32_t major = ORPHEUS_ABI_MAJOR;
      if (std::string_view(module.legacy_symbol).find("session") != std::string_view::npos) {
        auto old_fn =
            reinterpret_cast<const orpheus_session_api_v1* (*)(uint32_t, uint32_t*, uint32_t*)>(
                legacy);
        auto new_fn =
            reinterpret_cast<const treefall_session_api_v1* (*)(uint32_t, uint32_t*, uint32_t*)>(
                treefall);
        if (old_fn(major, nullptr, nullptr) != new_fn(major, nullptr, nullptr))
          throw std::runtime_error("Legacy and Treefall session tables differ");
      } else if (std::string_view(module.legacy_symbol).find("clipgrid") !=
                 std::string_view::npos) {
        auto old_fn =
            reinterpret_cast<const orpheus_clipgrid_api_v1* (*)(uint32_t, uint32_t*, uint32_t*)>(
                legacy);
        auto new_fn =
            reinterpret_cast<const treefall_clipgrid_api_v1* (*)(uint32_t, uint32_t*, uint32_t*)>(
                treefall);
        if (old_fn(major, nullptr, nullptr) != new_fn(major, nullptr, nullptr))
          throw std::runtime_error("Legacy and Treefall clipgrid tables differ");
      } else {
        auto old_fn =
            reinterpret_cast<const orpheus_render_api_v1* (*)(uint32_t, uint32_t*, uint32_t*)>(
                legacy);
        auto new_fn =
            reinterpret_cast<const treefall_render_api_v1* (*)(uint32_t, uint32_t*, uint32_t*)>(
                treefall);
        if (old_fn(major, nullptr, nullptr) != new_fn(major, nullptr, nullptr))
          throw std::runtime_error("Legacy and Treefall render tables differ");
      }
    }
    constexpr std::array<std::pair<const char*, const char*>, 3> error_symbols{{
        {"orpheus_status_to_string", "treefall_status_to_string"},
        {"orpheus_set_logger", "treefall_set_logger"},
        {"orpheus_set_telemetry_callback", "treefall_set_telemetry_callback"},
    }};
    for (const auto& names : error_symbols) {
      PrintResolution(names.first, LoadSymbol(first, names.first));
      PrintResolution(names.second, LoadSymbol(first, names.second));
    }
    auto old_logger = reinterpret_cast<void (*)(orpheus_log_callback, void*)>(
        LoadSymbol(first, "orpheus_set_logger"));
    auto new_logger = reinterpret_cast<void (*)(treefall_log_callback, void*)>(
        LoadSymbol(first, "treefall_set_logger"));
    old_logger(LoaderLogger, nullptr);
    new_logger(nullptr, nullptr);
    auto old_telemetry = reinterpret_cast<void (*)(orpheus_telemetry_callback, void*)>(
        LoadSymbol(first, "orpheus_set_telemetry_callback"));
    auto new_telemetry = reinterpret_cast<void (*)(treefall_telemetry_callback, void*)>(
        LoadSymbol(first, "treefall_set_telemetry_callback"));
    new_telemetry(LoaderTelemetry, nullptr);
    old_telemetry(nullptr, nullptr);
    auto status = reinterpret_cast<const char* (*)(orpheus_status)>(
        LoadSymbol(first, "treefall_status_to_string"));
    auto old_status = reinterpret_cast<const char* (*)(orpheus_status)>(
        LoadSymbol(first, "orpheus_status_to_string"));
    if (std::string(status(ORPHEUS_STATUS_OK)) != old_status(ORPHEUS_STATUS_OK))
      throw std::runtime_error("Treefall and legacy status exports disagree");

    auto old_session =
        reinterpret_cast<const orpheus_session_api_v1* (*)(uint32_t, uint32_t*, uint32_t*)>(
            LoadSymbol(first, "orpheus_session_abi_v1"));
    auto new_session =
        reinterpret_cast<const treefall_session_api_v1* (*)(uint32_t, uint32_t*, uint32_t*)>(
            LoadSymbol(first, "treefall_session_abi_v1"));
    if (old_session(ORPHEUS_ABI_MAJOR, nullptr, nullptr) !=
        new_session(ORPHEUS_ABI_MAJOR, nullptr, nullptr))
      throw std::runtime_error("Legacy and Treefall session tables differ");
    orpheus_session_handle handle = nullptr;
    if (old_session(ORPHEUS_ABI_MAJOR, nullptr, nullptr)->create(&handle) != ORPHEUS_STATUS_OK)
      throw std::runtime_error("Cross-name session create failed");
    new_session(ORPHEUS_ABI_MAJOR, nullptr, nullptr)->destroy(handle);
  } catch (const std::exception& ex) {
    std::cerr << "ABI link smoke failed: " << ex.what() << std::endl;
    for (ModuleHandle handle : handles)
      CloseModule(handle);
    return 1;
  }
  for (ModuleHandle handle : handles)
    CloseModule(handle);
  return 0;
}
