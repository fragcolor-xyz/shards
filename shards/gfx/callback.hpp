#ifndef D4FAACAD_67D3_4842_BC01_84850DA74232
#define D4FAACAD_67D3_4842_BC01_84850DA74232

#include <memory>
#include <vector>
#include <shared_mutex>
#include <mutex>

namespace gfx {

template <typename... TArgs> struct CallbackRegistry;

template <typename... TArgs> struct CallbackHandleImpl {
  std::weak_ptr<CallbackRegistry<TArgs...>> reg;

  virtual void call(TArgs... args) = 0;

  virtual ~CallbackHandleImpl() {
    if (auto r = reg.lock()) {
      std::unique_lock lock(r->lock);
      for (auto it = r->callbacks.begin(); it != r->callbacks.end();) {
        if (it->lock().get() == this) {
          it = r->callbacks.erase(it);
          break;
        } else {
          ++it;
        }
      }
    }
  }
};

template <typename... TArgs> using CallbackHandle = std::shared_ptr<CallbackHandleImpl<TArgs...>>;

template <typename... TArgs> struct CallbackRegistry : public std::enable_shared_from_this<CallbackRegistry<TArgs...>> {
  using Item = CallbackHandleImpl<TArgs...>;
  using ItemPtr = std::shared_ptr<Item>;

  std::shared_mutex lock;
  std::vector<std::weak_ptr<Item>> callbacks;

  void call(TArgs... args) {
    std::shared_lock<std::shared_mutex> lock(this->lock);

    for (auto it = callbacks.begin(); it != callbacks.end();) {
      if (auto cb = it->lock()) {
        cb->call(args...);
        ++it;
      } else {
        it = callbacks.erase(it);
      }
    }
  }

  template <typename T, typename... TArgs1> std::shared_ptr<T> emplace(TArgs1 &&...args) {
    std::unique_lock lock(this->lock);

    auto e = std::make_shared<T>(std::forward<TArgs1...>(args...));
    e->reg = this->shared_from_this();
    callbacks.push_back(e);
    return e;
  }
};

} // namespace gfx

#endif /* D4FAACAD_67D3_4842_BC01_84850DA74232 */
