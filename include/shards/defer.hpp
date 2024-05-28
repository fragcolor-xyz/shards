#ifndef E7D21D67_62C8_45B2_901C_1A3B13735513
#define E7D21D67_62C8_45B2_901C_1A3B13735513

namespace shards {

// https://godbolt.org/z/I72ctd
template <class Function> struct Defer {
  Function _f;
  Defer(Function &&f) : _f(f) {}
  ~Defer() { _f(); }
};

#define DEFER_NAME(uniq) _defer##uniq
#define DEFER_DEF(uniq, body) ::shards::Defer DEFER_NAME(uniq)([&]() { body; })
#define DEFER(body) DEFER_DEF(__LINE__, body)

} // namespace shards

#endif /* E7D21D67_62C8_45B2_901C_1A3B13735513 */
