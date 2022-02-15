#pragma once
#include <cassert>
#include <memory>

namespace gfx {

template <typename T> struct TWithContextData;
struct Context;

// NOTE: Your destructor should manually call releaseConditional!!!
struct ContextData : public std::enable_shared_from_this<ContextData> {
private:
  Context *context = nullptr;

public:
  ContextData() = default;
  virtual ~ContextData() = default;

  void releaseConditional();
  Context &getContext() {
    assert(context);
    return *context;
  }

protected:
  virtual void release() = 0;

private:
  template <typename T> friend struct TWithContextData;
  void bindToContext(Context &context);
  void unbindFromContext();
};

// Manages initialization/cleanup of context-specific data T attached to this object
template <typename T> struct TWithContextData {
  std::shared_ptr<T> contextData;

  void createContextDataConditional(Context &context) {
    if (!contextData) {
      contextData = std::make_shared<T>();
      contextData->bindToContext(context);
      initContextData(context, *contextData.get());
    } else {
      assert(&contextData->getContext() == &context);
    }
    updateContextData(context, *contextData.get());
  }

protected:
  virtual void initContextData(Context &context, T &contextData) = 0;
  virtual void updateContextData(Context &context, T &contextData) {}
};

} // namespace gfx