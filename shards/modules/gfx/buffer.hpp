#ifndef B99D55E1_FCD9_449B_AB84_9952AC1CFFB0
#define B99D55E1_FCD9_449B_AB84_9952AC1CFFB0

#include "gfx.hpp"
#include <gfx/shader/types.hpp>

namespace gfx {
struct ShaderTypeParser {
  shader::Type root;

  shader::Type parse(const SHVar &v) { return visitVar(v); }

private:
  shader::Type visitStructTable(shards::TableVar &tv) {
    shader::StructType st;
    for (auto &[k, v] : tv) {
      if (k.valueType != SHType::String)
        throw std::runtime_error("Expected a string key");
      st->addField(k.payload.stringValue, visitVar(v));
    }
    return st;
  }
  shader::Type visitTable(shards::TableVar &tv) {
    // If the table has a 'type' entry, assume it's a type definition
    auto type = tv.find("type");
    if (!type) {
      return visitStructTable(tv);
    }

    auto base = visitVar(*type);

    auto unsigned_ = tv.find("unsigned");
    if (unsigned_) {
      if (unsigned_->valueType != SHType::Bool)
        throw std::runtime_error("Expected 'unsigned' to be a boolean");
      if (unsigned_->payload.boolValue) {
        auto baseNt = std::get_if<shader::NumType>(&base);
        if (!baseNt || baseNt->baseType != ShaderFieldBaseType::Int32)
          throw std::runtime_error("Only integer values can be unsigned");
        baseNt->baseType = ShaderFieldBaseType::UInt32;
      }
    }

    auto atomic = tv.find("atomic");
    if (atomic) {
      if (atomic->valueType != SHType::Bool)
        throw std::runtime_error("Expected 'atomic' to be a boolean");
      if (atomic->payload.boolValue) {
        if (base != shader::Types::Int32 && base != shader::Types::UInt32)
          throw std::runtime_error("Only Int or UInt values can be atomic");
        std::get<shader::NumType>(base).atomic = true;
      }
    }

    auto length = tv.find("length");
    if (length) {
      if (length->valueType != SHType::Int)
        throw std::runtime_error("Expected 'length' to be an integer");
      if (length->payload.intValue <= 0)
        throw std::runtime_error("Expected 'length' to be a positive integer");
      auto arrType = std::get_if<shader::ArrayType>(&base);
      if (!arrType)
        throw std::runtime_error("'length' can only be applied to array types");
      (*arrType)->fixedLength = length->payload.intValue;
    }

    return base;
  }

  shader::Type visitSeq(shards::SeqVar &tv) {
    if (tv.size() != 1)
      throw std::runtime_error("Expected a single element in the sequence");
    return shader::ArrayType(visitVar(tv[0]));
  }

  shader::Type visitSimpleType(SHTypeInfo *ti) {
    switch (ti->basicType) {
    case SHType::Bool:
      return shader::Types::Bool;
    case SHType::Int:
      return shader::Types::Int32;
    case SHType::Int2:
      return shader::Types::Int2;
    case SHType::Int3:
      return shader::Types::Int3;
    case SHType::Int4:
      return shader::Types::Int4;
    case SHType::Float:
      return shader::Types::Float;
    case SHType::Float2:
      return shader::Types::Float2;
    case SHType::Float3:
      return shader::Types::Float3;
    case SHType::Float4:
      return shader::Types::Float4;
    case SHType::Seq: {
      if (ti->seqTypes.len != 1)
        throw std::runtime_error("Expected a single element in the sequence");
      auto base = visitSimpleType(ti->seqTypes.elements);
      return shader::ArrayType(base);
    } break;
    default:
      break;
    }
    throw std::runtime_error(fmt::format("Invalid shader type: {}", *ti));
  }

  shader::Type visitVar(const SHVar &v) {
    if (v.valueType == SHType::Table) {
      return visitTable((shards::TableVar &)v);
    } else if (v.valueType == SHType::Seq) {
      return visitSeq((shards::SeqVar &)v);
    } else if (v.valueType == SHType::Type) {
      return visitSimpleType(v.payload.typeValue);
    } else {
      throw std::runtime_error(fmt::format("Invalid shader type {}: {}", v.valueType, v));
    }
  }
};

struct DerivedLayoutInfo {
  shader::StructLayout layout;
  shader::StructLayoutLookup structLayoutLookup;
  std::optional<shader::LayoutRef> runtimeSizedField;

  static inline DerivedLayoutInfo make(shader::AddressSpace addressSpace, const shader::StructType &st) {
    DerivedLayoutInfo out;

    shader::StructLayoutBuilder slb(addressSpace);
    slb.pushFromStruct(st);
    out.layout = slb.finalize();
    out.structLayoutLookup = std::move(slb.getStructMap());
    shader::LayoutTraverser lt(out.layout, out.structLayoutLookup);
    if (out.layout.isRuntimeSized) {
      out.runtimeSizedField = lt.findRuntimeSizedArray();
    }
    return out;
  }
};

inline DerivedLayoutInfo parseStructType(shader::Type& parsedType, const SHVar &type, shader::AddressSpace addressSpace) {
  parsedType = ShaderTypeParser{}.parse(type);
  if (auto st = std::get_if<shader::StructType>(&parsedType))
    return DerivedLayoutInfo::make(addressSpace, *st);
  else {
    throw std::runtime_error("Expected a struct type");
  }
}
} // namespace gfx

#endif /* B99D55E1_FCD9_449B_AB84_9952AC1CFFB0 */
