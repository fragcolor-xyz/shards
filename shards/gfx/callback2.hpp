#ifndef D94B5FD4_B792_4231_9B40_F030D279C15B
#define D94B5FD4_B792_4231_9B40_F030D279C15B

#include <memory>
#include <vector>
#include <shared_mutex>
#include <mutex>
#include <boost/container/small_vector.hpp>

namespace gfx {
struct CallbackRegistryBase : public std::enable_shared_from_this<CallbackRegistryBase> {
  virtual void removeCallback(void *handle) = 0;
};

struct CallbackHandleImplBase {
  std::weak_ptr<CallbackRegistryBase> registry;

  virtual ~CallbackHandleImplBase() {
    if (auto r = registry.lock()) {
      r->removeCallback(this);
    }
  }
};

template <typename HandleImpl>
  requires std::is_base_of_v<CallbackHandleImplBase, HandleImpl>
struct CallbackRegistry final : public CallbackRegistryBase {
  using Item = HandleImpl;
  using CallbackHandle = std::shared_ptr<Item>;
  using ItemPtr = std::shared_ptr<Item>;

  std::shared_mutex lock;
  std::vector<std::weak_ptr<Item>> callbacks;

  template <typename T> void forEach(T &&action_) {
    std::shared_lock<std::shared_mutex> lock(this->lock);
    boost::container::small_vector<std::weak_ptr<Item>, 32> removalQueue;

    for (auto it = callbacks.begin(); it != callbacks.end();) {
      if (auto cb = it->lock()) {
        action_(*cb.get());
        ++it;
      } else {
        removalQueue.push_back(*it);
      }
    }

    if (removalQueue.size() > 0) {
      lock.release();
      // it.
      // it = callbacks.erase(it);
    }
  }

  template <typename T, typename... TArgs1>
  std::shared_ptr<T> emplace(TArgs1 &&...args)
    requires std::is_base_of_v<HandleImpl, T>
  {
    std::unique_lock lock(this->lock);

    auto e = std::make_shared<T>(std::forward<TArgs1...>(args...));
    e->registry = this->shared_from_this();
    callbacks.push_back(e);
    return e;
  }

  void removeCallback(void *handle) override {
    CallbackHandleImplBase *base = reinterpret_cast<CallbackHandleImplBase *>(handle);
    std::unique_lock lock(this->lock);
    for (auto it = callbacks.begin(); it != callbacks.end();) {
      if (it->lock().get() == base) {
        it = callbacks.erase(it);
        break;
      } else {
        ++it;
      }
    }
  }
};
} // namespace gfx

#endif /* D94B5FD4_B792_4231_9B40_F030D279C15B */
