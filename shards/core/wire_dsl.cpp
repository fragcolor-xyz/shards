#include <shards/wire_dsl.hpp>

namespace shards {
Var::Var(const Wire &wire) : Var(wire.weakRef()) {}

Weave &Weave::shard(std::string_view name, std::vector<Var> params) {
  auto blk = createShard(name.data());
  if (!blk) {
    SHLOG_ERROR("Shard {} was not found", name);
    throw SHException("Shard not found");
  }

  blk->setup(blk);

  const auto psize = params.size();
  for (size_t i = 0; i < psize; i++) {
    // skip Any, as they mean default value
    if (params[i] != Var::Any)
      blk->setParam(blk, int(i), &params[i]);
  }

  _shards.emplace_back(blk);
  return *this;
}

Weave &Weave::let(Var value) {
  auto blk = createShard("Const");
  blk->setup(blk);
  blk->setParam(blk, 0, &value);
  _shards.emplace_back(blk);
  return *this;
}

Wire::Wire(std::string_view name) : _wire(SHWire::make(name)) {}

Wire &Wire::looped(bool looped) {
  _wire->looped = looped;
  return *this;
}

Wire &Wire::unsafe(bool unsafe) {
  _wire->unsafe = unsafe;
  return *this;
}

Wire &Wire::stackSize(size_t stackSize) {
#if SH_CORO_NEED_STACK_MEM
  _wire->stackSize = stackSize;
#endif
  return *this;
}

Wire &Wire::name(std::string_view name) {
  _wire->name = name;
  return *this;
}

Wire &Wire::shard(std::string_view name, std::vector<Var> params) {
  auto blk = createShard(name.data());
  if (!blk) {
    SHLOG_ERROR("Shard {} was not found", name);
    throw SHException("Shard not found");
  }

  blk->setup(blk);

  const auto psize = params.size();
  for (size_t i = 0; i < psize; i++) {
    // skip Any, as they mean default value
    if (params[i] != Var::Any)
      blk->setParam(blk, int(i), &params[i]);
  }

  _wire->addShard(blk);
  return *this;
}

Wire &Wire::let(Var value) {
  auto blk = createShard("Const");
  blk->setup(blk);
  blk->setParam(blk, 0, &value);
  _wire->addShard(blk);
  return *this;
}

SHWireRef Wire::weakRef() const { return SHWire::weakRef(_wire); }
} // namespace shards