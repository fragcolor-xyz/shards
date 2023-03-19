#include "coro.hpp"

void SHCoro::init(shards::coroutine &&handle) { this->handle = std::move(handle); }

void SHCoro::resume() { handle.resume(); }

shards::coroutine SHCoro::suspend() { assert(false); }

shards::coroutine SHCoro::yield() { co_await std::suspend_always{}; }

SHCoro::~SHCoro() {}

SHCoro::operator bool() const { return !handle.done(); }
