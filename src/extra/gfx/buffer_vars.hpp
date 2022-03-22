#include <chainblocks.hpp>
#include <gfx/error_utils.hpp>
#include <gfx/mesh.hpp>
#include <magic_enum.hpp>
#include <vector>

namespace gfx {

inline MeshVertexAttribute getAttributeVertexFormat(const CBVar &attribute) {
  switch (attribute.valueType) {
  case CBType::Float2:
    return MeshVertexAttribute("", 2, VertexAttributeType::Float32);
  case CBType::Float3:
    return MeshVertexAttribute("", 3, VertexAttributeType::Float32);
  case CBType::Float4:
    return MeshVertexAttribute("", 4, VertexAttributeType::Float32);
  case CBType::Int4:
  case CBType::Color:
    return MeshVertexAttribute("", 4, VertexAttributeType::UNorm8);
  default:
    throw formatException("Unsupported vertex attribute type {} {}", magic_enum::enum_name(attribute.valueType),
                          uint8_t(attribute.valueType));
  }
}

inline void validateVertexFormat(const std::vector<MeshVertexAttribute> &format, const CBVar &vertices) {
  if (vertices.valueType != CBType::Seq) {
    throw formatException("Vertices parameter is not a sequence");
  }

  const CBSeq &seq = vertices.payload.seqValue;

  if ((seq.len % format.size()) != 0) {
    throw formatException("Invalid number of vertices ({}), needs to be a "
                          "multiple of {} (based on layout)",
                          format.size());
  }

  for (size_t i = 0; i < seq.len;) {
    for (size_t attrIndex = 0; attrIndex < format.size(); attrIndex++, i++) {
      const CBVar &attribute = seq.elements[i];

      if (!format[attrIndex].isSameDataFormat(getAttributeVertexFormat(attribute))) {
        throw formatException("Vertex attribute {} (index: {}) does not match "
                              "the previous format",
                              attrIndex, i);
      }
    }
  }
}

inline void determineVertexFormat(std::vector<MeshVertexAttribute> &outAttributes, size_t numAttributes, const CBVar &vertices) {
  if (vertices.valueType != CBType::Seq) {
    throw formatException("Vertices parameter is not a sequence");
  }

  const CBSeq &seq = vertices.payload.seqValue;
  if (seq.len == 0)
    return;

  if (seq.len < numAttributes) {
    throw formatException("Not enough vertices for given vertex layout");
  }

  for (size_t i = 0; i < numAttributes; i++) {
    const CBVar &attribute = seq.elements[i];

    switch (attribute.valueType) {
    case CBType::Float2:
      outAttributes.emplace_back(MeshVertexAttribute("", 2, VertexAttributeType::Float32));
      break;
    case CBType::Float3:
      outAttributes.emplace_back(MeshVertexAttribute("", 3, VertexAttributeType::Float32));
      break;
    case CBType::Float4:
      outAttributes.emplace_back(MeshVertexAttribute("", 4, VertexAttributeType::Float32));
      break;
    case CBType::Int4:
    case CBType::Color:
      outAttributes.emplace_back(MeshVertexAttribute("", 4, VertexAttributeType::UNorm8));
      break;
    default:
      throw formatException("Unsupported vertex attribute {}", magic_enum::enum_name(attribute.valueType));
    }
  }
}

inline void validateIndexFormat(IndexFormat format, const CBVar &indices) {
  if (indices.valueType != CBType::Seq) {
    throw formatException("Indices parameter is not a sequence");
  }

  const CBSeq &indicesSeq = indices.payload.seqValue;
  if ((indicesSeq.len % 3) != 0) {
    throw formatException("Invalid number of inidices ({}), needs to be a multiple of 3", indicesSeq.len);
  }
}

inline IndexFormat determineIndexFormat(const CBVar &indices) {
  IndexFormat format = IndexFormat::UInt16;

  if (indices.valueType != CBType::Seq) {
    throw formatException("Indices parameter is not a sequence");
  }

  const CBSeq &seq = indices.payload.seqValue;
  if (seq.len == 0)
    return format;

  for (size_t i = 0; i < seq.len; i++) {
    const CBVar &elem = seq.elements[i];
    if (elem.valueType != CBType::Int) {
      throw formatException("Invalid index format at {}, {}", i, magic_enum::enum_name(elem.valueType));
    }

    if (elem.payload.intValue > UINT16_MAX) {
      format = IndexFormat::UInt32;
    }
  }

  return format;
}

template <typename Appender> inline void packIntoVertexBuffer(Appender &output, const CBVar &var);
template <typename Appender> inline void packIntoVertexBuffer(Appender &output, const CBSeq &seq) {
  for (size_t i = 0; i < seq.len; i++) {
    packIntoVertexBuffer(output, seq.elements[i]);
  }
}
template <typename Appender> inline void packIntoVertexBuffer(Appender &output, const CBVar &var) {
  switch (var.valueType) {
  case CBType::Float2:
    output(&var.payload.float2Value, sizeof(float) * 2);
    break;
  case CBType::Float3:
    output(&var.payload.float3Value, sizeof(float) * 3);
    break;
  case CBType::Float4:
    output(&var.payload.float4Value, sizeof(float) * 4);
    break;
  case CBType::Int4: {
    if (var.payload.int4Value[0] < 0 || var.payload.int4Value[0] > 255 || var.payload.int4Value[1] < 0 ||
        var.payload.int4Value[1] > 255 || var.payload.int4Value[2] < 0 || var.payload.int4Value[2] > 255 ||
        var.payload.int4Value[3] < 0 || var.payload.int4Value[3] > 255) {
      throw formatException("Int4 vertex attribute value must be between 0 and 255");
    }
    CBColor value;
    value.r = uint8_t(var.payload.int4Value[0]);
    value.g = uint8_t(var.payload.int4Value[1]);
    value.b = uint8_t(var.payload.int4Value[2]);
    value.a = uint8_t(var.payload.int4Value[3]);
    output(&value, sizeof(uint8_t) * 4);
  } break;
  case CBType::Color:
    output(&var.payload.colorValue.r, sizeof(uint8_t) * 4);
    break;
  case CBType::Seq:
    packIntoVertexBuffer(output, var.payload.seqValue);
    break;
  default:
    throw formatException("Cannot pack value of type {} into vertex buffer", magic_enum::enum_name(var.valueType));
  }
}

template <typename Appender> inline void packIntoIndexBuffer(Appender &output, IndexFormat format, const CBVar &var);
template <typename Appender> inline void packIntoIndexBuffer(Appender &output, IndexFormat format, const CBSeq &seq) {
  for (size_t i = 0; i < seq.len; i++) {
    packIntoIndexBuffer(output, format, seq.elements[i]);
  }
}
template <typename Appender> inline void packIntoIndexBuffer(Appender &output, IndexFormat format, const CBVar &var) {
  if (var.valueType == CBType::Int) {
    if (format == IndexFormat::UInt16) {
      output(&var.payload.intValue, sizeof(uint16_t));
    } else {
      output(&var.payload.intValue, sizeof(uint32_t));
    }
  } else if (var.valueType == CBType::Seq) {
    packIntoIndexBuffer(output, format, var.payload.seqValue);
  } else {
    throw formatException("Cannot pack value of type {} into index buffer", magic_enum::enum_name(var.valueType));
  }
}
} // namespace gfx
