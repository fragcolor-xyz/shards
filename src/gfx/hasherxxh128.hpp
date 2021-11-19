#include "hash.hpp"
#include "linalg.hpp"
#include "xxh3.h"
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace gfx {

template<typename TVisitor>
struct HasherXXH128 {
	XXH3_state_t state;
	HasherXXH128() { XXH3_128bits_reset(&state); }

	Hash128 getDigest() {
		XXH128_hash_t hash = XXH3_128bits_digest(&state);
		return Hash128(hash.low64, hash.high64);
	}

	template <typename T> struct is_valid { static constexpr int value = 1; };
	template <typename T, typename T1 = void> struct has_hash_function {};
	template <typename T> struct has_hash_function<T, std::enable_if_t<is_valid<decltype(((T *)1)->hashStatic(*(HasherXXH128 *)1))>::value>> { static constexpr int value = 1; };

	void operator()(const void *data, size_t length) { XXH3_128bits_update(&state, data, length); }
	void operator()(const float4 &v) { (*this)(&v.x, sizeof(float) * 4); }

	template <typename TVal> std::enable_if_t<std::is_pod<TVal>::value> operator()(const TVal &val) { (*this)(&val, sizeof(val)); }
	template <typename TVal> std::enable_if_t<has_hash_function<TVal>::value> operator()(const TVal &val) { val.hashStatic(*this); }
	void operator()(const std::string &str) { (*this)(str.data(), str.size()); }
	template <typename TVal> void operator()(const std::vector<TVal> &vec) {
		for (auto &item : vec) {
			(*this)(item);
		}
	}
	template <typename TVal> void operator()(const std::unordered_map<std::string, TVal> &map) {
		for (auto &pair : map) {
			(*this)(pair.first);
			(*this)(pair.second);
		}
	}
};
} // namespace gfx
