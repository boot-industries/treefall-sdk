#ifndef REAPER_API_LOADER_HPP
#define REAPER_API_LOADER_HPP

#include "reaper_plugin.h"
#include "reaper_plugin_functions.h"
#include <utility>

class ReaperAPILoader {
public:
  using get_api_t = void *(*)(const char *);

  template <typename Sig> class Function;

  template <typename Ret, typename... Args> class Function<Ret(Args...)> {
  public:
    using Fn = Ret (*)(Args...);
    Function() : fn_(nullptr) {}
    Function(get_api_t api, const char *name) { load(api, name); }
    bool load(get_api_t api, const char *name) {
      fn_ = api ? reinterpret_cast<Fn>(api(name)) : nullptr;
      return fn_ != nullptr;
    }
    bool valid() const { return fn_ != nullptr; }
    explicit operator bool() const { return valid(); }
    Ret operator()(Args... args) const { return fn_(args...); }

  private:
    Fn fn_;
  };

  explicit ReaperAPILoader(reaper_plugin_info_t *rec) : api_(nullptr), ok_(false) {
    if (!rec || rec->caller_version != REAPER_PLUGIN_VERSION || !rec->GetFunc)
      return;
    api_ = rec->GetFunc;
    ok_ = (REAPERAPI_LoadAPI(api_) == 0);
  }
  ~ReaperAPILoader() = default;

  bool ok() const { return ok_; }
  explicit operator bool() const { return ok_; }

  template <typename Sig> Function<Sig> load(const char *name) const {
    return Function<Sig>(api_, name);
  }

private:
  get_api_t api_;
  bool ok_;
};

#endif
