#include "ops_internal.hpp"
#include "ops.hpp"

MAKE_LOGGABLE(CBVar, var, os) {
  switch (var.valueType) {
  case EndOfBlittableTypes:
    break;
  case None:
    os << "*None*";
    break;
  case CBType::Any:
    os << "*Any*";
    break;
  case Object:
    os << "Object: 0x" << std::hex
       << reinterpret_cast<uintptr_t>(var.payload.objectValue) << std::dec;
    break;
  case Chain:
    os << "Chain: 0x" << std::hex
       << reinterpret_cast<uintptr_t>(var.payload.chainValue) << std::dec;
    break;
  case Bytes:
    os << "Bytes: 0x" << std::hex
       << reinterpret_cast<uintptr_t>(var.payload.bytesValue)
       << " size: " << std::dec << var.payload.bytesSize;
    break;
  case Array:
    os << "Array: 0x" << std::hex
       << reinterpret_cast<uintptr_t>(var.payload.arrayValue.elements)
       << " size: " << std::dec << var.payload.arrayValue.len
       << " of: " << type2Name(var.innerType);
    break;
  case Enum:
    os << "Enum: " << var.payload.enumValue;
    break;
  case Bool:
    os << (var.payload.boolValue ? "true" : "false");
    break;
  case Int:
    os << var.payload.intValue;
    break;
  case Int2:
    os << "(";
    for (auto i = 0; i < 2; i++) {
      if (i == 0)
        os << var.payload.int2Value[i];
      else
        os << ", " << var.payload.int2Value[i];
    }
    os << ")";
    break;
  case Int3:
    os << "(";
    for (auto i = 0; i < 3; i++) {
      if (i == 0)
        os << var.payload.int3Value[i];
      else
        os << ", " << var.payload.int3Value[i];
    }
    os << ")";
    break;
  case Int4:
    os << "(";
    for (auto i = 0; i < 4; i++) {
      if (i == 0)
        os << var.payload.int4Value[i];
      else
        os << ", " << var.payload.int4Value[i];
    }
    os << ")";
    break;
  case Int8:
    os << "(";
    for (auto i = 0; i < 8; i++) {
      if (i == 0)
        os << var.payload.int8Value[i];
      else
        os << ", " << var.payload.int8Value[i];
    }
    os << ")";
    break;
  case Int16:
    os << "(";
    for (auto i = 0; i < 16; i++) {
      if (i == 0)
        os << int(var.payload.int16Value[i]);
      else
        os << ", " << int(var.payload.int16Value[i]);
    }
    os << ")";
    break;
  case Float:
    os << var.payload.floatValue;
    break;
  case Float2:
    os << "(";
    for (auto i = 0; i < 2; i++) {
      if (i == 0)
        os << var.payload.float2Value[i];
      else
        os << ", " << var.payload.float2Value[i];
    }
    os << ")";
    break;
  case Float3:
    os << "(";
    for (auto i = 0; i < 3; i++) {
      if (i == 0)
        os << var.payload.float3Value[i];
      else
        os << ", " << var.payload.float3Value[i];
    }
    os << ")";
    break;
  case Float4:
    os << "(";
    for (auto i = 0; i < 4; i++) {
      if (i == 0)
        os << var.payload.float4Value[i];
      else
        os << ", " << var.payload.float4Value[i];
    }
    os << ")";
    break;
  case Color:
    os << int(var.payload.colorValue.r) << ", " << int(var.payload.colorValue.g)
       << ", " << int(var.payload.colorValue.b) << ", "
       << int(var.payload.colorValue.a);
    break;
  case Block:
    os << "Block: " << var.payload.blockValue->name(var.payload.blockValue);
    break;
  case CBType::String:
    os << var.payload.stringValue;
    break;
  case ContextVar:
    os << "ContextVariable: " << var.payload.stringValue;
    break;
  case CBType::Path:
    os << "Path: " << var.payload.stringValue;
    break;
  case Image:
    os << "Image";
    os << " Width: " << var.payload.imageValue.width;
    os << " Height: " << var.payload.imageValue.height;
    os << " Channels: " << (int)var.payload.imageValue.channels;
    break;
  case Seq:
    os << "[";
    for (uint32_t i = 0; i < var.payload.seqValue.len; i++) {
      const auto &v = var.payload.seqValue.elements[i];
      if (i == 0)
        os << v;
      else
        os << ", " << v;
    }
    os << "]";
    break;
  case Table:
    os << "{";
    auto &ta = var.payload.tableValue;
    struct iterdata {
      bool first;
      el::base::type::ostream_t *os;
    } data;
    data.first = true;
    data.os = &os;
    ta.api->tableForEach(
        ta,
        [](const char *key, CBVar *value, void *_data) {
          auto data = (iterdata *)_data;
          if (data->first) {
            *data->os << key << ": " << *value;
            data->first = false;
          } else {
            *data->os << ", " << key << ": " << *value;
          }
          return true;
        },
        &data);
    os << "}";
    break;
  }
  return os;
}

MAKE_LOGGABLE(CBTypeInfo, t, os) {
  os << type2Name(t.basicType);
  if (t.basicType == CBType::Seq) {
    os << " [";
    for (uint32_t i = 0; i < t.seqTypes.len; i++) {
      os << "(" << t.seqTypes.elements[i] << ")";
      if (i < (t.seqTypes.len - 1)) {
        os << " ";
      }
    }
    os << "]";
  } else if (t.basicType == CBType::Table) {
    if (t.table.types.len == t.table.keys.len) {
      os << " {";
      for (uint32_t i = 0; i < t.table.types.len; i++) {
        os << "\"" << t.table.keys.elements[i] << "\" ";
        os << "(" << t.table.types.elements[i] << ")";
        if (i < (t.table.types.len - 1)) {
          os << " ";
        }
      }
      os << "}";
    } else {
      os << " [";
      for (uint32_t i = 0; i < t.table.types.len; i++) {
        os << "(" << t.table.types.elements[i] << ")";
        if (i < (t.table.types.len - 1)) {
          os << " ";
        }
      }
      os << "]";
    }
  }
  return os;
}

MAKE_LOGGABLE(CBTypesInfo, ts, os) {
  os << "[";
  for (uint32_t i = 0; i < ts.len; i++) {
    os << "(" << ts.elements[i] << ")";
    if (i < (ts.len - 1)) {
      os << " ";
    }
  }
  os << "]";
  return os;
}