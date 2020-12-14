#include "ops_internal.hpp"

#define XXH_INLINE_ALL
#include <xxhash.h>

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
        os << var.payload.int16Value[i];
      else
        os << ", " << var.payload.int16Value[i];
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

uint64_t chainblocks::hash(const CBVar &var, bool initiator) {
  // once per thread, we leak this in the end but does not matter
  static thread_local XXH3_state_t *tHashState{nullptr};

  static_assert(std::is_same<uint64_t, XXH64_hash_t>::value,
                "XXH64_hash_t is not uint64_t");

  if (initiator) {
    if (tHashState == nullptr) {
      tHashState = XXH3_createState();
    } else {
      auto error = XXH3_64bits_reset(tHashState);
      assert(error == XXH_OK);
    }
  } else {
    assert(tHashState);
  }

  auto error =
      XXH3_64bits_update(tHashState, &var.valueType, sizeof(var.valueType));
  assert(error == XXH_OK);

  switch (var.valueType) {
  case Bytes: {
    error = XXH3_64bits_update(tHashState, var.payload.bytesValue,
                               size_t(var.payload.bytesSize));
    assert(error == XXH_OK);
  } break;
  case Path:
  case ContextVar:
  case String: {
    error = XXH3_64bits_update(tHashState, var.payload.stringValue,
                               size_t(var.payload.stringLen > 0
                                          ? var.payload.stringLen
                                          : strlen(var.payload.stringValue)));
    assert(error == XXH_OK);
  } break;
  case Image: {
    error = XXH3_64bits_update(tHashState, &var.payload, sizeof(CBVarPayload));
    assert(error == XXH_OK);
    auto pixsize = 1;
    if ((var.payload.imageValue.flags & CBIMAGE_FLAGS_16BITS_INT) ==
        CBIMAGE_FLAGS_16BITS_INT)
      pixsize = 2;
    else if ((var.payload.imageValue.flags & CBIMAGE_FLAGS_32BITS_FLOAT) ==
             CBIMAGE_FLAGS_32BITS_FLOAT)
      pixsize = 4;
    error = XXH3_64bits_update(
        tHashState, var.payload.imageValue.data,
        size_t(var.payload.imageValue.channels * var.payload.imageValue.width *
               var.payload.imageValue.height * pixsize));
    assert(error == XXH_OK);
  } break;
  case Seq: {
    for (uint32_t i = 0; i < var.payload.seqValue.len; i++) {
      hash(var.payload.seqValue.elements[i], false); // false == update
    }
  } break;
  default: {
    error = XXH3_64bits_update(tHashState, &var.payload, sizeof(CBVarPayload));
    assert(error == XXH_OK);
  }
  }
  return XXH3_64bits_digest(tHashState);
}