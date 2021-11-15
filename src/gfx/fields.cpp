#include "fields.hpp"
#include "linalg/linalg.h"
#include "spdlog/fmt/bundled/core.h"
#include <cassert>
#include <spdlog/fmt/fmt.h>
#include <type_traits>

namespace gfx {

void packFieldVariant(const FieldVariant &variant, std::vector<uint8_t> &outBuffer) {
	std::visit(
		[&](auto &&arg) {
			using T = std::decay_t<decltype(arg)>;
			size_t offset = outBuffer.size();
			if constexpr (std::is_same_v<T, float>) {
				size_t len = sizeof(float) * 1;
				outBuffer.resize(outBuffer.size() + len);
				memcpy(&outBuffer[offset], &arg, len);
			} else if constexpr (std::is_same_v<T, float2>) {
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
FieldType getFieldVariantType(const FieldVariant &variant) {
	FieldType result = FieldType::Float;
	std::visit(
		[&](auto &&arg) {
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same_v<T, float>) {
				result = FieldType::Float;
			} else if constexpr (std::is_same_v<T, float2>) {
				result = FieldType::Float2;
			} else if constexpr (std::is_same_v<T, float3>) {
				result = FieldType::Float3;
			} else if constexpr (std::is_same_v<T, float4>) {
				result = FieldType::Float4;
			} else if constexpr (std::is_same_v<T, float4x4>) {
				result = FieldType::Float4x4;
			} else if constexpr (std::is_same_v<T, TexturePtr>) {
				result = FieldType::Texture2D;
			} else {
				assert(false);
			}
		},
		variant);
	return result;
}

bool isBasicFieldType(const FieldType &type) {
	switch (type) {
	case FieldType::Texture2D:
		return false;
	default:
		return true;
	}
}

const char *getBasicFieldGLSLName(FieldType type) {
	switch (type) {
	case FieldType::Float:
		return "float";
		break;
	case FieldType::Float2:
		return "vec2";
		break;
	case FieldType::Float3:
		return "vec3";
		break;
	case FieldType::Float4:
		return "vec4";
		break;
	case FieldType::Float4x4:
		return "mat4";
		break;
	default:
		return nullptr;
	}
}

static void formatFloat4(const float4 &v, std::string &out) { out += fmt::format("vec4({:.8f}, {:.8f}, {:.8f}, {:.8f})", v.x, v.y, v.z, v.w); };
static void formatFloat4x4(const float4x4 &v, std::string &out) {
	out += "mtxFromCols4(";
	formatFloat4(v.x, out);
	formatFloat4(v.y, out);
	formatFloat4(v.z, out);
	formatFloat4(v.w, out);
	out += ")";
};

bool getBasicFieldGLSLValue(FieldType type, const FieldVariant &variant, std::string &out) {
	switch (type) {
	case FieldType::Float:
		if (const float *v = std::get_if<float>(&variant)) {
			out += fmt::format("float({})", *v);
			return true;
		}
		break;
	case FieldType::Float2:
		if (const float2 *v = std::get_if<float2>(&variant)) {
			out += fmt::format("vec2({:.8f}, {:.8f})", v->x, v->y);
			return true;
		}
		break;
	case FieldType::Float3:
		if (const float3 *v = std::get_if<float3>(&variant)) {
			out += fmt::format("vec3({:.8f}, {:.8f}, {:.8f})", v->x, v->y, v->z);
			return true;
		}
		break;
	case FieldType::Float4:
		if (const float4 *v = std::get_if<float4>(&variant)) {
			formatFloat4(*v, out);
			return true;
		}
		break;
	case FieldType::Float4x4:
		if (const float4x4 *v = std::get_if<float4x4>(&variant)) {
			formatFloat4x4(*v, out);
			return true;
		}
		break;
	default:
		break;
	}
	return false;
}

void getBasicFieldGLSLDefaultValue(FieldType type, std::string &out) {
	switch (type) {
	case FieldType::Float:
		out += "0.0";
		break;
	case FieldType::Float2:
		out += "vec2(0.0, 0.0)";
		break;
	case FieldType::Float3:
		out += "vec3(0.0, 0.0, 0.0)";
		break;
	case FieldType::Float4:
		out += "vec4(0.0, 0.0, 0.0, 0.0)";
		break;
	case FieldType::Float4x4:
		static float4x4 identity = linalg::identity;
		formatFloat4x4(identity, out);
		break;
	default:
		break;
	}
}

} // namespace gfx
