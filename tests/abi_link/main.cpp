// SPDX-License-Identifier: MIT
#include "orpheus/abi.h"

#include <array>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#elif defined(__APPLE__)
  #include <dlfcn.h>
#else
  #include <dlfcn.h>
#endif

#ifndef ORP_BUILD_SHARED_CORE
#  define ORP_BUILD_SHARED_CORE 0
#endif

namespace fs = std::filesystem;

namespace {

#if defined(_WIN32)
using ModuleHandle = HMODULE;
constexpr std::string_view kSharedExtension = ".dll";

ModuleHandle LoadModule(const fs::path &path) {
  return ::LoadLibraryW(path.wstring().c_str());
}

void *LoadSymbol(ModuleHandle handle, const char *name) {
  return reinterpret_cast<void *>(::GetProcAddress(handle, name));
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
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
  std::string message = "unknown";
  if (length != 0 && buffer != nullptr) {
    std::wstring_view wide(buffer, length);
    while (!wide.empty() &&
           (wide.back() == L'\r' || wide.back() == L'\n' ||
            wide.back() == L' ')) {
      wide = wide.substr(0, wide.size() - 1);
    }
    message.assign(wide.begin(), wide.end());
  }
  if (buffer != nullptr) {
    ::LocalFree(buffer);
  }
  return message;
}
#else
using ModuleHandle = void *;

ModuleHandle LoadModule(const fs::path &path) {
  const std::string encoded = path.string();
  return dlopen(encoded.c_str(), RTLD_NOW);
}

void *LoadSymbol(ModuleHandle handle, const char *name) {
  return dlsym(handle, name);
}

void CloseModule(ModuleHandle handle) {
  if (handle != nullptr) {
    dlclose(handle);
  }
}

std::string LastErrorString() {
  const char *message = dlerror();
  return message != nullptr ? std::string(message) : std::string("unknown");
}

#  if defined(__APPLE__)
constexpr std::string_view kSharedExtension = ".dylib";
#  else
constexpr std::string_view kSharedExtension = ".so";
#  endif
#endif

struct ModuleInfo {
  const char *library_name;
  const char *factory_symbol;
};

template <typename Fn>
void PrintResolution(const std::string &symbol, Fn &&fn) {
  const void *address = reinterpret_cast<const void *>(fn());
  if (address == nullptr) {
    throw std::runtime_error("Factory returned null pointer for " + symbol);
  }
  std::cout << "Resolved " << symbol << " -> " << address << std::endl;
}

}  // namespace

int main() {
#if !ORP_BUILD_SHARED_CORE
  std::cout << "abi_link: skipping (shared core disabled)" << std::endl;
  return 0;
#endif

  const fs::path library_dir(ORPHEUS_ABI_LINK_DIR);
  std::cout << "Loading Orpheus ABI libraries from " << library_dir
            << std::endl;

  const std::array<ModuleInfo, 3> modules{{
      {ORPHEUS_SESSION_LIB, "orpheus_session_abi_v1"},
      {ORPHEUS_CLIPGRID_LIB, "orpheus_clipgrid_abi_v1"},
      {ORPHEUS_RENDER_LIB, "orpheus_render_abi_v1"},
  }};

  std::vector<ModuleHandle> handles;
  handles.reserve(modules.size());

  try {
    for (const auto &module : modules) {
      const fs::path library_path = library_dir / module.library_name;
      if (library_path.extension() != kSharedExtension) {
        throw std::runtime_error("Expected shared library extension " +
                                 std::string(kSharedExtension) + " for " +
                                 library_path.string());
      }
      std::cout << "Opening " << library_path << std::endl;
      ModuleHandle handle = LoadModule(library_path);
      if (handle == nullptr) {
        throw std::runtime_error("Failed to load " + library_path.string() +
                                 ": " + LastErrorString());
      }
      handles.push_back(handle);

      void *symbol = LoadSymbol(handle, module.factory_symbol);
      if (symbol == nullptr) {
        throw std::runtime_error("Failed to resolve " +
                                 std::string(module.factory_symbol) +
                                 " from " + library_path.string() + ": " +
                                 LastErrorString());
      }

      if (module.factory_symbol == std::string("orpheus_session_abi_v1")) {
        auto fn = reinterpret_cast<const orpheus_session_v1 *(*)()>(symbol);
        PrintResolution(module.factory_symbol, fn);

        void *negotiate_symbol = LoadSymbol(handle, "orpheus_negotiate_abi");
        if (negotiate_symbol == nullptr) {
          throw std::runtime_error("Failed to resolve orpheus_negotiate_abi from " +
                                   library_path.string() + ": " +
                                   LastErrorString());
        }

        auto negotiate_fn =
            reinterpret_cast<const orpheus_abi_negotiator *(*)()>(
                negotiate_symbol);
        const auto *negotiator = negotiate_fn();
        if (negotiator == nullptr || negotiator->negotiate == nullptr) {
          throw std::runtime_error("Negotiator ABI unavailable");
        }
        const auto negotiated =
            negotiator->negotiate({orpheus::kCurrentAbi.major,
                                   orpheus::kCurrentAbi.minor});
        std::cout << "Negotiated ABI " << negotiated.major << "."
                  << negotiated.minor << std::endl;
      } else if (module.factory_symbol ==
                 std::string("orpheus_clipgrid_abi_v1")) {
        auto fn = reinterpret_cast<const orpheus_clipgrid_v1 *(*)()>(symbol);
        PrintResolution(module.factory_symbol, fn);
      } else if (module.factory_symbol ==
                 std::string("orpheus_render_abi_v1")) {
        auto fn = reinterpret_cast<const orpheus_render_v1 *(*)()>(symbol);
        PrintResolution(module.factory_symbol, fn);
      }
    }
  } catch (const std::exception &ex) {
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
