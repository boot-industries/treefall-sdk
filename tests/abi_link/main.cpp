// SPDX-License-Identifier: MIT
#include "orpheus/abi.h"

#include <array>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
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
  const char* factory_symbol;
};

void PrintResolution(const std::string& symbol, const void* address) {
  if (address == nullptr) {
    throw std::runtime_error("Factory returned null pointer for " + symbol);
  }
  std::cout << "Resolved " << symbol << " -> " << address << std::endl;
}

} // namespace

int main() {
#if !ORP_BUILD_SHARED_CORE
  std::cout << "abi_link: skipping (shared core disabled)" << std::endl;
  return 0;
#endif

  const fs::path library_dir(ORPHEUS_ABI_LINK_DIR);
  std::cout << "Loading Orpheus ABI libraries from " << library_dir << std::endl;

  const std::array<ModuleInfo, 3> modules{{
      {ORPHEUS_SESSION_LIB, "orpheus_session_abi_v1"},
      {ORPHEUS_CLIPGRID_LIB, "orpheus_clipgrid_abi_v1"},
      {ORPHEUS_RENDER_LIB, "orpheus_render_abi_v1"},
  }};

  std::vector<ModuleHandle> handles;
  handles.reserve(modules.size());

  try {
    for (const auto& module : modules) {
      const fs::path library_path = library_dir / module.library_name;
      if (library_path.extension() != kSharedExtension) {
        throw std::runtime_error("Expected shared library extension " +
                                 std::string(kSharedExtension) + " for " + library_path.string());
      }
      std::cout << "Opening " << library_path << std::endl;
      ModuleHandle handle = LoadModule(library_path);
      if (handle == nullptr) {
        throw std::runtime_error("Failed to load " + library_path.string() + ": " +
                                 LastErrorString());
      }
      handles.push_back(handle);

      void* symbol = LoadSymbol(handle, module.factory_symbol);
      if (symbol == nullptr) {
        throw std::runtime_error("Failed to resolve " + std::string(module.factory_symbol) +
                                 " from " + library_path.string() + ": " + LastErrorString());
      }

      PrintResolution(module.factory_symbol, symbol);
      if (module.factory_symbol == std::string("orpheus_session_abi_v1")) {
        auto fn =
            reinterpret_cast<const orpheus_session_api_v1* (*)(uint32_t, uint32_t*, uint32_t*)>(
                symbol);
        uint32_t major = 0;
        uint32_t minor = 0;
        const auto* abi = fn(ORPHEUS_ABI_MAJOR, &major, &minor);
        if (abi == nullptr) {
          throw std::runtime_error("Session ABI negotiation returned null");
        }
        std::cout << "Negotiated session ABI " << major << "." << minor << " caps=0x" << std::hex
                  << abi->caps << std::dec << std::endl;
      } else if (module.factory_symbol == std::string("orpheus_clipgrid_abi_v1")) {
        auto fn =
            reinterpret_cast<const orpheus_clipgrid_api_v1* (*)(uint32_t, uint32_t*, uint32_t*)>(
                symbol);
        uint32_t major = 0;
        uint32_t minor = 0;
        const auto* abi = fn(ORPHEUS_ABI_MAJOR, &major, &minor);
        if (abi == nullptr) {
          throw std::runtime_error("Clipgrid ABI negotiation returned null");
        }
        std::cout << "Negotiated clipgrid ABI " << major << "." << minor << " caps=0x" << std::hex
                  << abi->caps << std::dec << std::endl;
      } else if (module.factory_symbol == std::string("orpheus_render_abi_v1")) {
        auto fn =
            reinterpret_cast<const orpheus_render_api_v1* (*)(uint32_t, uint32_t*, uint32_t*)>(
                symbol);
        uint32_t major = 0;
        uint32_t minor = 0;
        const auto* abi = fn(ORPHEUS_ABI_MAJOR, &major, &minor);
        if (abi == nullptr) {
          throw std::runtime_error("Render ABI negotiation returned null");
        }
        std::cout << "Negotiated render ABI " << major << "." << minor << " caps=0x" << std::hex
                  << abi->caps << std::dec << std::endl;
      }
    }
  } catch (const std::exception& ex) {
    std::cerr << "ABI link smoke failed: " << ex.what() << std::endl;
    for (ModuleHandle handle : handles) {
      CloseModule(handle);
    }
    return 1;
  }

  for (ModuleHandle handle : handles) {
    CloseModule(handle);
  }

  return 0;
}
