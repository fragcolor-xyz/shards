#pragma once

#include "hash.hpp"
#include "linalg.hpp"
#include "xxh3.h"
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace gfx {

struct HasherDefaultVisitor {
	template <typename T, typename H> static constexpr auto applies(...) -> decltype(std::declval<T>().hash(*(H *)0), bool()) { return true; }

	template <typename T, typename THasher> void operator()(const T &value, THasher &&hasher) { value.hash(hasher); }
};

struct HashStaticVistor {
	template <typename T, typename H> static constexpr auto applies(...) -> decltype(std::declval<T>().hashStatic(*(H *)0), bool()) { return true; }

	template <typename T, typename THasher> void operator()(const T &val, THasher &hasher) { val.hashStatic(hasher); }
};

template <typename TVisitor = HasherDefaultVisitor> struct HasherXXH128 {
	XXH3_state_t state = {};

	HasherXXH128() {
		XXH3_INITSTATE(&state);
		reset();
	}

	void reset() { XXH3_128bits_reset(&state); }

	Hash128 getDigest() {
		XXH128_hash_t hash = XXH3_128bits_digest(&state);
		return Hash128(hash.low64, hash.high64);
	}

	void operator()(const void *data, size_t length) { XXH3_128bits_update(&state, data, length); }
	void operator()(const Hash128 &v) { (*this)(&v, sizeof(Hash128)); }
	template <typename T> void operator()(const linalg::vec<T, 4> &v) { (*this)(&v.x, sizeof(T) * 4); }
	template <typename TVal> void operator()(const std::optional<TVal> &v) {
		bool has_value = v.has_value();
		(*this)(has_value);
		if (has_value)
			(*this)(v.value());
	}

	template <typename TVal> void operator()(const TVal &val) {
		static_assert(canVisit<TVal>(0) || std::is_pod<TVal>::value, "Type can not be hashed");
		if constexpr (canVisit<TVal>(0)) {
			TVisitor{}(val, *this);
		} else if constexpr (std::is_pod<TVal>::value) {
			(*this)(&val, sizeof(val));
		}
	}
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

	template <typename TVal> static constexpr auto canVisit(int) -> decltype(TVisitor::template applies<TVal, HasherXXH128>(), bool()) {
		return TVisitor::template applies<TVal, HasherXXH128>(0);
	}
	template <typename TVal> static constexpr auto canVisit(char) -> bool { return false; }
};
} // namespace gfx
