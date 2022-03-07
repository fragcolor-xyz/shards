#pragma once
#include <cassert>
#include <memory>

namespace gfx {

struct Context;
struct WithContextData : public std::enable_shared_from_this<WithContextData> {
  Context *context = nullptr;

  virtual ~WithContextData() = default;
  virtual void createContextDataCondtional(Context *context) = 0;
  virtual void releaseContextDataCondtional() = 0;

protected:
  void bindToContext(Context *context);
  void unbindFromContext();
};

// Manages initialization/cleanup of context-specific data T attached to this object
// NOTE: your destructor should call releaseContextDataCondtional still!
template <typename T> struct TWithContextData : public WithContextData {
  T contextData;

  virtual void createContextDataCondtional(Context *context) final {
    if (!this->context) {
      bindToContext(context);
      createContextData();
    } else {
      assert(this->context == context);
    }
  }

  virtual void releaseContextDataCondtional() final {
    if (context) {
      releaseContextData();
      unbindFromContext();
    }
  }

protected:
  virtual void createContextData() = 0;
  virtual void releaseContextData() = 0;
};

#define WGPU_SAFE_RELEASE(_fn, _x) \
  if (_x) {                        \
    _fn(_x);                       \
    _x = nullptr;                  \
  }

} // namespace gfx