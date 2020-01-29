/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#ifndef CB_OPS_INTERNAL_HPP
#define CB_OPS_INTERNAL_HPP

#include <easylogging++.h>
#include <ops.hpp>

inline MAKE_LOGGABLE(CBVar, var, os) {
  switch (var.valueType) {
  case EndOfBlittableTypes:
    break;
  case None:
    if (var.payload.chainState == Continue)
      os << "*None*";
    else if (var.payload.chainState == CBChainState::Stop)
      os << "*ChainStop*";
    else if (var.payload.chainState == CBChainState::Restart)
      os << "*ChainRestart*";
    else if (var.payload.chainState == CBChainState::Return)
      os << "*ChainReturn*";
    else if (var.payload.chainState == CBChainState::Rebase)
      os << "*ChainRebase*";
    break;
  case CBType::TypeInfo:
    if (var.payload.typeInfoValue)
      os << type2Name(var.payload.typeInfoValue->basicType);
    else
      os << "None";
    break;
  case CBType::Any:
    os << "*Any*";
    break;
  case Object:
    os << "Object: 0x" << std::hex
       << reinterpret_cast<uintptr_t>(var.payload.objectValue) << std::dec;
    break;
  case Node:
    os << "Node: 0x" << std::hex
       << reinterpret_cast<uintptr_t>(var.payload.nodeValue) << std::dec;
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
    for (auto i = 0; i < 2; i++) {
      if (i == 0)
        os << var.payload.int2Value[i];
      else
        os << ", " << var.payload.int2Value[i];
    }
    break;
  case Int3:
    for (auto i = 0; i < 3; i++) {
      if (i == 0)
        os << var.payload.int3Value[i];
      else
        os << ", " << var.payload.int3Value[i];
    }
    break;
  case Int4:
    for (auto i = 0; i < 4; i++) {
      if (i == 0)
        os << var.payload.int4Value[i];
      else
        os << ", " << var.payload.int4Value[i];
    }
    break;
  case Int8:
    for (auto i = 0; i < 8; i++) {
      if (i == 0)
        os << var.payload.int8Value[i];
      else
        os << ", " << var.payload.int8Value[i];
    }
    break;
  case Int16:
    for (auto i = 0; i < 16; i++) {
      if (i == 0)
        os << var.payload.int16Value[i];
      else
        os << ", " << var.payload.int16Value[i];
    }
    break;
  case Float:
    os << var.payload.floatValue;
    break;
  case Float2:
    for (auto i = 0; i < 2; i++) {
      if (i == 0)
        os << var.payload.float2Value[i];
      else
        os << ", " << var.payload.float2Value[i];
    }
    break;
  case Float3:
    for (auto i = 0; i < 3; i++) {
      if (i == 0)
        os << var.payload.float3Value[i];
      else
        os << ", " << var.payload.float3Value[i];
    }
    break;
  case Float4:
    for (auto i = 0; i < 4; i++) {
      if (i == 0)
        os << var.payload.float4Value[i];
      else
        os << ", " << var.payload.float4Value[i];
    }
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
    os << "*ContextVariable*: " << var.payload.stringValue;
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
      if (i == 0)
        os << var.payload.seqValue.elements[i];
      else
        os << ", " << var.payload.seqValue.elements[i];
    }
    os << "]";
    break;
  case Table:
    os << "{";
    for (ptrdiff_t i = 0; i < stbds_shlen(var.payload.tableValue); i++) {
      if (i == 0)
        os << var.payload.tableValue[i].key << ": "
           << var.payload.tableValue[i].value;
      else
        os << ", " << var.payload.tableValue[i].key << ": "
           << var.payload.tableValue[i].value;
    }
    os << "}";
    break;
  case Vector: {
    os << "[";
    CBVar vv{};
    vv.valueType = var.payload.vectorType;
    for (uint32_t i = 0; i < var.payload.vectorSize; i++) {
      vv.payload = var.payload.vectorValue[i];
      if (i == 0)
        os << vv;
      else
        os << ", " << vv;
    }
    os << "]";
  } break;
  case List: {
    os << "(";
    auto next = var.payload.listValue.value;
    while (next) {
      if (var.payload.listValue.next) {
        os << next << " ";
        next = var.payload.listValue.next;
      } else {
        os << *next;
      }
    }
    os << ")";
  } break;
  }
  return os;
}

#endif
