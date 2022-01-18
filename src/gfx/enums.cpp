#include "enums.hpp"
#include <magic_enum.hpp>
#include <stdint.h>

namespace gfx {
size_t getVertexAttributeTypeSize(const VertexAttributeType &type) {
	switch (type) {
	case VertexAttributeType::UInt8:
	case VertexAttributeType::Int8:
	case VertexAttributeType::UNorm8:
	case VertexAttributeType::SNorm8:
		return sizeof(uint8_t);
	case VertexAttributeType::UInt16:
	case VertexAttributeType::Int16:
	case VertexAttributeType::UNorm16:
	case VertexAttributeType::SNorm16:
		return sizeof(uint16_t);
	case VertexAttributeType::UInt32:
	case VertexAttributeType::Int32:
	case VertexAttributeType::UNorm32:
	case VertexAttributeType::SNorm32:
		return sizeof(uint32_t);
	case VertexAttributeType::Float16:
		return sizeof(uint16_t);
	case VertexAttributeType::Float32:
		return sizeof(float);
	default:
		assert(false);
		return 0;
	}
}
size_t getIndexFormatSize(const IndexFormat &type) {
	switch (type) {
	case IndexFormat::UInt16:
		return sizeof(uint16_t);
	case IndexFormat::UInt32:
		return sizeof(uint32_t);
	default:
		assert(false);
		return 0;
	}
}
} // namespace gfx