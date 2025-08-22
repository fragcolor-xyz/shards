#ifndef B6A28F45_D99B_4F7B_B4D3_155504E6ECA3
#define B6A28F45_D99B_4F7B_B4D3_155504E6ECA3

#include <stdint.h>
#include <type_traits>

#ifndef __builtin_fabs
#include <cmath>
#define __builtin_fabs(x) std::abs(x)
#define __builtin_sqrt(x) std::sqrt(x)
#define __builtin_log(x) std::log(x)
#define __builtin_sin(x) std::sin(x)
#define __builtin_cos(x) std::cos(x)
#define __builtin_exp(x) std::exp(x)
#define __builtin_tanh(x) std::tanh(x)
#endif

#ifndef __builtin_unreachable
#include <stdlib.h>
#define __builtin_unreachable() (__assume(0))
#endif

namespace shards {

template <typename T, int N> struct TShardVector {
  static constexpr int StorageSize = N;
  __declspec(align(16)) T v[N];
  static constexpr TShardVector All(T v) {
    TShardVector r;
    for (auto i = 0; i < N; i++) {
      r[i] = v;
    }
    return r;
  }
  T &operator[](int i) { return v[i]; }
  const T &operator[](int i) const { return v[i]; }
};

} // namespace shards

typedef shards::TShardVector<int64_t, 2> SHInt2;
typedef shards::TShardVector<int32_t, 4> SHInt3;
typedef shards::TShardVector<int32_t, 4> SHInt4;
typedef shards::TShardVector<int16_t, 8> SHInt8;
typedef shards::TShardVector<int8_t, 16> SHInt16;

typedef shards::TShardVector<double, 2> SHFloat2;
typedef shards::TShardVector<float, 4> SHFloat3;
typedef shards::TShardVector<float, 4> SHFloat4;

namespace shards {

template <typename T>
concept TIsVectorType = T::StorageSize != 0;
template <typename T>
concept TIsIntegralVectorType = TIsVectorType<T> && std::is_integral_v<std::decay_t<decltype(T::v[0])>>;

// +, -, *, /, unary minus, ^, |, &, ~, %.
template <typename T>
inline T operator+(T a, T b)
  requires TIsVectorType<T>
{
  T r;
  for (auto i = 0; i < T::StorageSize; i++) {
    r[i] = a[i] + b[i];
  }
  return r;
}
template <typename T>
inline T operator-(T a, T b)
  requires TIsVectorType<T>
{
  T r;
  for (auto i = 0; i < T::StorageSize; i++) {
    r[i] = a[i] - b[i];
  }
  return r;
}
template <typename T>
inline T operator*(T a, T b)
  requires TIsVectorType<T>
{
  T r;
  for (auto i = 0; i < T::StorageSize; i++) {
    r[i] = a[i] * b[i];
  }
  return r;
}

template <typename T>
inline T operator/(T a, T b)
  requires TIsVectorType<T>
{
  T r;
  for (auto i = 0; i < T::StorageSize; i++) {
    r[i] = a[i] / b[i];
  }
  return r;
}

template <typename T>
inline T operator-(T a)
  requires TIsVectorType<T>
{
  T r;
  for (auto i = 0; i < T::StorageSize; i++) {
    r[i] = -a[i];
  }
  return r;
}

template <typename T>
inline T operator^(T a, T b)
  requires TIsIntegralVectorType<T>
{
  T r;
  for (auto i = 0; i < T::StorageSize; i++) {
    r[i] = a[i] ^ b[i];
  }
  return r;
}

template <typename T>
inline T operator|(T a, T b)
  requires TIsIntegralVectorType<T>
{
  T r;
  for (auto i = 0; i < T::StorageSize; i++) {
    r[i] = a[i] | b[i];
  }
  return r;
}

template <typename T>
inline T operator&(T a, T b)
  requires TIsIntegralVectorType<T>
{
  T r;
  for (auto i = 0; i < T::StorageSize; i++) {
    r[i] = a[i] & b[i];
  }
  return r;
}

template <typename T>
inline T operator~(T a)
  requires TIsIntegralVectorType<T>
{
  T r;
  for (auto i = 0; i < T::StorageSize; i++) {
    r[i] = ~a[i];
  }
  return r;
}

template <typename T>
inline T operator%(T a, T b)
  requires TIsIntegralVectorType<T>
{
  T r;
  for (auto i = 0; i < T::StorageSize; i++) {
    r[i] = a[i] % b[i];
  }
  return r;
}

// Compound assignment operators
template <typename T>
inline T &operator+=(T &a, T b)
  requires TIsVectorType<T>
{
  for (auto i = 0; i < T::StorageSize; i++) {
    a[i] += b[i];
  }
  return a;
}

template <typename T>
inline T &operator-=(T &a, T b)
  requires TIsVectorType<T>
{
  for (auto i = 0; i < T::StorageSize; i++) {
    a[i] -= b[i];
  }
  return a;
}

template <typename T>
inline T &operator*=(T &a, T b)
  requires TIsVectorType<T>
{
  for (auto i = 0; i < T::StorageSize; i++) {
    a[i] *= b[i];
  }
  return a;
}

template <typename T>
inline T &operator/=(T &a, T b)
  requires TIsVectorType<T>
{
  for (auto i = 0; i < T::StorageSize; i++) {
    a[i] /= b[i];
  }
  return a;
}

template <typename T>
inline T &operator^=(T &a, T b)
  requires TIsIntegralVectorType<T>
{
  for (auto i = 0; i < T::StorageSize; i++) {
    a[i] ^= b[i];
  }
  return a;
}

template <typename T>
inline T &operator|=(T &a, T b)
  requires TIsIntegralVectorType<T>
{
  for (auto i = 0; i < T::StorageSize; i++) {
    a[i] |= b[i];
  }
  return a;
}

template <typename T>
inline T &operator&=(T &a, T b)
  requires TIsIntegralVectorType<T>
{
  for (auto i = 0; i < T::StorageSize; i++) {
    a[i] &= b[i];
  }
  return a;
}

template <typename T>
inline T &operator%=(T &a, T b)
  requires TIsIntegralVectorType<T>
{
  for (auto i = 0; i < T::StorageSize; i++) {
    a[i] %= b[i];
  }
  return a;
}

template <typename T>
inline T operator<<(T a, T b)
  requires TIsIntegralVectorType<T>
{
  T result = a;
  for (auto i = 0; i < T::StorageSize; i++) {
    result[i] <<= b[i];
  }
  return result;
}

template <typename T>
inline T operator>>(T a, T b)
  requires TIsIntegralVectorType<T>
{
  T result = a;
  for (auto i = 0; i < T::StorageSize; i++) {
    result[i] >>= b[i];
  }
  return result;
}

// Comparison operators

template <size_t N> struct TVecCompareResult {
  using type = TShardVector<int32_t, N>;
};
template <> struct TVecCompareResult<2> {
  using type = TShardVector<int64_t, 2>;
};
template <> struct TVecCompareResult<8> {
  using type = TShardVector<int16_t, 8>;
};
template <> struct TVecCompareResult<16> {
  using type = TShardVector<int8_t, 16>;
};

template <typename T> using TVecCompareType = typename TVecCompareResult<T::StorageSize>::type;

template <typename T>
inline TVecCompareType<T> operator<=(T a, T b)
  requires TIsVectorType<T>
{
  auto res = TVecCompareType<T>::All(1);
  for (auto i = 0; i < T::StorageSize; i++) {
    if (a[i] > b[i])
      res[i] = 0;
  }
  return res;
}

template <typename T>
inline TVecCompareType<T> operator==(T a, T b)
  requires TIsVectorType<T>
{
  auto res = TVecCompareType<T>::All(1);
  for (auto i = 0; i < T::StorageSize; i++) {
    if (a[i] != b[i])
      res[i] = 0;
  }
  return res;
}

template <typename T>
inline TVecCompareType<T> operator!=(T a, T b)
  requires TIsVectorType<T>
{
  auto res = TVecCompareType<T>::All(1);
  for (auto i = 0; i < T::StorageSize; i++) {
    if (a[i] == b[i])
      res[i] = 0;
  }
  return res;
}

} // namespace shards

#endif /* B6A28F45_D99B_4F7B_B4D3_155504E6ECA3 */