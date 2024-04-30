#ifndef C51922C9_A0A1_463B_86AB_B786E4F226F9
#define C51922C9_A0A1_463B_86AB_B786E4F226F9

#include "runtime.hpp"
#include "serialization.hpp"
#include <type_traits>
#include <concepts>
#include <deque>
#include <unordered_set>
#include <string>
#include <mutex>

namespace shards {

template <typename TData>
concept WireData = requires(TData data) {
  { data.wire } -> std::assignable_from<std::shared_ptr<SHWire>>;
};

template <typename TData>
concept WireDataDeps = requires(TData data) {
  { data.wires.clear() };
  { data.wires.push_back(std::shared_ptr<SHWire>()) };
};

template <typename T> struct WireDoppelgangerPool {
  WireDoppelgangerPool(SHWireRef master) {
    // Never call this from setParam or earlier...
    auto vwire = shards::Var(master);
    std::stringstream stream;
    Writer w(stream);
    Serialization serializer;
    serializer.serialize(vwire, w);
    _wireStr = stream.str();
  }

  // notice users should stop wires themselves, we might want wires to persist
  // after this object lifetime
  void stopAll()
    requires WireData<T>
  {
    for (auto &item : _pool) {
      stop(item->wire.get());
      _avail.emplace(item.get());
    }
  }

  template <class Composer, typename Anything>
  T *acquire(Composer &composer, Anything *anything)
    requires WireData<T>
  {
    ZoneScoped;

    std::unique_lock<std::mutex> lock(_poolMutex);
    if (_avail.size() == 0) {
      lock.unlock();

      Serialization serializer;
      std::stringstream stream(_wireStr);
      Reader r(stream);
      SHVar vwire{};
      serializer.deserialize(r, vwire);
      auto wire = SHWire::sharedFromRef(vwire.payload.wireValue);
      destroyVar(vwire);

      lock.lock();
      auto &fresh = _pool.emplace_back(std::make_shared<T>());
      auto index = _pool.size();
      lock.unlock();

      fresh->wire = wire;
      fresh->wire->name = fmt::format("{}-{}", fresh->wire->name, index);
      if constexpr (WireDataDeps<T>) {
        fresh->wires.clear();
        for (auto &w : serializer.wires) {
          fresh->wires.push_back(SHWire::sharedFromRef(w.second));
        }
      }
      composer.compose(wire.get(), anything, false);

      return fresh.get();
    } else {
      auto res = _avail.extract(_avail.begin());
      lock.unlock();

      auto &value = res.value();
      composer.compose(value->wire.get(), anything, true);

      return value;
    }
  }

  void release(T *wire) {
    shassert(wire != nullptr && "Releasing a null wire?");

    std::unique_lock<std::mutex> _l(_poolMutex);

    _avail.emplace(wire);
  }

  size_t available() const { return _avail.size(); }

private:
  WireDoppelgangerPool() = delete;

  struct Writer {
    std::stringstream &stream;
    Writer(std::stringstream &stream) : stream(stream) {}
    void operator()(const uint8_t *buf, size_t size) { stream.write((const char *)buf, size); }
  };

  struct Reader {
    std::stringstream &stream;
    Reader(std::stringstream &stream) : stream(stream) {}
    void operator()(uint8_t *buf, size_t size) { stream.read((char *)buf, size); }
  };

  // keep our pool in a deque in order to keep them alive
  // so users don't have to worry about lifetime
  // just release when possible
  std::mutex _poolMutex;
  std::deque<std::shared_ptr<T>> _pool;
  std::unordered_set<T *> _avail;
  std::string _wireStr;
};
} // namespace shards

#endif /* C51922C9_A0A1_463B_86AB_B786E4F226F9 */
