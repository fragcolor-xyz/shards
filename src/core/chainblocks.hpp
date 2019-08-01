#pragma once

#include "chainblocks.h"

// Included 3rdparty
#include "3rdparty/json.hpp"
#include "3rdparty/parallel_hashmap/phmap.h"
#include "3rdparty/easylogging++.h"

inline MAKE_LOGGABLE(CBVar, var, os) {
  switch(var.valueType)
  {
    case EndOfBlittableTypes:
      break;
    case None:
      if(var.payload.chainState == Continue)
        os << "*None*";
      else if(var.payload.chainState == Stop)
        os << "*ChainStop*";
      else if(var.payload.chainState == Restart)
        os << "*ChainRestart*";
      break;
    case Any:
      os << "*Any*";
      break;
    case Object:
      os << "Object: " << reinterpret_cast<uintptr_t>(var.payload.objectValue);
      break;
    case Enum:
      os << "Enum: " << var.payload.enumValue;
      break;
    case Bool:
      os << var.payload.boolValue;
      break;
    case Int:
      os << var.payload.intValue;
      break;
    case Int2:
      for(auto i = 0; i < 2; i++)
      {
        if(i == 0)
          os << var.payload.int2Value[i];
        else
          os << ", " << var.payload.int2Value[i];
      }
      break;
    case Int3:
      for(auto i = 0; i < 3; i++)
      {
        if(i == 0)
          os << var.payload.int3Value[i];
        else
          os << ", " << var.payload.int3Value[i];
      }
      break;
    case Int4:
      for(auto i = 0; i < 4; i++)
      {
        if(i == 0)
          os << var.payload.int4Value[i];
        else
          os << ", " << var.payload.int4Value[i];
      }
      break;
    case Int8:
      for(auto i = 0; i < 8; i++)
      {
        if(i == 0)
          os << var.payload.int8Value[i];
        else
          os << ", " << var.payload.int8Value[i];
      }
      break;
    case Int16:
      for(auto i = 0; i < 16; i++)
      {
        if(i == 0)
          os << var.payload.int16Value[i];
        else
          os << ", " << var.payload.int16Value[i];
      }
      break;
    case Float:
      os << var.payload.floatValue;
      break;
    case Float2:
      for(auto i = 0; i < 2; i++)
      {
        if(i == 0)
          os << var.payload.float2Value[i];
        else
          os << ", " << var.payload.float2Value[i];
      }
      break;
    case Float3:
      for(auto i = 0; i < 3; i++)
      {
        if(i == 0)
          os << var.payload.float3Value[i];
        else
          os << ", " << var.payload.float3Value[i];
      }
      break;
    case Float4:
      for(auto i = 0; i < 4; i++)
      {
        if(i == 0)
          os << var.payload.float4Value[i];
        else
          os << ", " << var.payload.float4Value[i];
      }
      break;
    case Color:
      os << var.payload.colorValue.r << ", " << var.payload.colorValue.g << ", " << var.payload.colorValue.b << ", " << var.payload.colorValue.a;
      break;
    case Chain:
      os << "Chain: " << reinterpret_cast<uintptr_t>(var.payload.chainValue);
      break;
    case Block:
      os << "Block: " << var.payload.blockValue->name(var.payload.blockValue);
      break;
    case String:
      os << var.payload.stringValue;
      break;
    case ContextVar:
      os << "*ContextVariable*: " << var.payload.stringValue;
      break;
    case Image:
      os << "Image";
      os << " Width: " << var.payload.imageValue.width;
      os << " Height: " << var.payload.imageValue.height;
      os << " Channels: " << var.payload.imageValue.channels;
      break;
    case Seq:
      os << "[";
      for(auto i = 0; i < stbds_arrlen(var.payload.seqValue); i++)
      {
        if(i == 0)
          os << var.payload.seqValue[i];
        else
          os << ", " << var.payload.seqValue[i];
      }
      os << "]";
      break;
    case Table:
      os << "{";
      for(auto i = 0; i < stbds_shlen(var.payload.tableValue); i++)
      {
        if(i == 0)
          os << var.payload.tableValue[i].key << ": " << var.payload.tableValue[i].value;
        else
          os << ", " << var.payload.tableValue[i].key << ": " << var.payload.tableValue[i].value;
      }
      os << "}";
      break;
  }
  return os;
}