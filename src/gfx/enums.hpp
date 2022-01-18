#pragma once

#include <string_view>

namespace gfx {

enum class PrimitiveType { TriangleList, TriangleStrip };
enum class WindingOrder { CW, CCW };
enum class ProgrammableGraphicsStage { Vertex, Fragment };
enum class VertexAttributeType { UInt8, Int8, UNorm8, SNorm8, UInt16, Int16, UNorm16, SNorm16, UInt32, Int32, UNorm32, SNorm32, Float16, Float32 };
enum class IndexFormat { UInt16, UInt32 };

size_t getVertexAttributeTypeSize(const VertexAttributeType &type);
size_t getIndexFormatSize(const IndexFormat& type);

} // namespace gfx
