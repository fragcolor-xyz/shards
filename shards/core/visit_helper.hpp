#ifndef E15D10A5_445F_4497_8E04_AD0D1E3D5FC6
#define E15D10A5_445F_4497_8E04_AD0D1E3D5FC6

namespace shards {
template <typename... TArgs> struct Overload : TArgs... {
  using TArgs::operator()...;
};
template <class... TArgs> Overload(TArgs...) -> Overload<TArgs...>;
} // namespace shards

#endif /* E15D10A5_445F_4497_8E04_AD0D1E3D5FC6 */
