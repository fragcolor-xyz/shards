#include "fields.hpp"
#include "linalg/linalg.h"
#include <type_traits>

namespace gfx {

void packFieldVariant(const FieldVariant &variant, std::vector<uint8_t> &outBuffer) {
	std::visit(
		[&](auto &&arg) {
			using T = std::decay_t<decltype(arg)>;
			size_t offset = outBuffer.size();
			if constexpr (std::is_same_v<T, float2>) {
				size_t len = sizeof(float) * 2;
				outBuffer.resize(outBuffer.size() + len);
				memcpy(&outBuffer[offset], &arg.x, len);
			} else if constexpr (std::is_same_v<T, float3>) {
				size_t len = sizeof(float) * 3;
				outBuffer.resize(outBuffer.size() + len);
				memcpy(&outBuffer[offset], &arg.x, len);
			} else if constexpr (std::is_same_v<T, float4>) {
				size_t len = sizeof(float) * 4;
				outBuffer.resize(outBuffer.size() + len);
				memcpy(&outBuffer[offset], &arg.x, len);
			} else if constexpr (std::is_same_v<T, float4x4>) {
				size_t len = sizeof(float) * 16;
				outBuffer.resize(outBuffer.size() + len);
				packFloat4x4(arg, (float *)&outBuffer[offset]);
			}
		},
		variant);
}

} // namespace gfx
