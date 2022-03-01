#pragma once

#include "chainblocks.h"
#include "common_types.hpp"
#include <cassert>
#include <map>
#include <vector>

namespace chainblocks {

enum class NumberType {
  Invalid = 0,
  UInt8,
  Int8,
  Int16,
  Int32,
  Int64,
  Float32,
  Float64,
};

std::map<CBType, NumberType> getCBTypeToNumberTypeMap();
const std::vector<NumberType> &getCBTypeToNumberTypeArrayMap();

inline NumberType cbTypeToNumberType(CBType type) {
  auto &mapping = getCBTypeToNumberTypeArrayMap();
  cbassert((size_t)type < mapping.size());
  return mapping[(size_t)type];
}

struct NumberConversionOutOfRangeEx {
  int64_t index;
  NumberConversionOutOfRangeEx(int64_t index) : index(index){};
};

// Convers from one number type to another
typedef void (*NumberConvertOneFunction)(const void *src, void *dst);

// Applies combined conversion and swizzle operation
// Throws NumberConversionOutOfRangeEx false on out of range index
typedef void (*NumberConvertMultipleSeqFunction)(const void *src, void *dst, size_t srcLen, const CBSeq &sequence);

typedef void (*NumberConvertParse)(void *dst, const char *input, char **inputEnd);

struct NumberConversion {
  size_t inStride;
  size_t outStride;
  NumberConvertOneFunction convertOne;
  NumberConvertMultipleSeqFunction convertMultipleSeq;
};

struct NumberConversionTable {
  std::vector<NumberConversion> conversions;

  const NumberConversion *get(NumberType toType) const {
    cbassert((size_t)toType < conversions.size());
    return &conversions[(size_t)toType];
  }
};

struct NumberTypeTraits {
  NumberType type = NumberType::Invalid;
  bool isInteger;
  size_t size;
  NumberConversionTable conversionTable;
  NumberConvertParse convertParse;
};

struct NumberTypeLookup {
private:
  std::vector<NumberTypeTraits> numberTypes;
  std::vector<const NumberTypeTraits *> cbTypeLookup;

public:
  NumberTypeLookup();

  const NumberTypeTraits *get(CBType type) {
    if (size_t(type) >= cbTypeLookup.size())
      return nullptr;

    return cbTypeLookup[size_t(type)];
  }

  const NumberTypeTraits *get(NumberType type) {
    if (size_t(type) >= numberTypes.size())
      return nullptr;

    const NumberTypeTraits &result = numberTypes[size_t(type)];
    if (result.type == NumberType::Invalid)
      return nullptr;

    return &result;
  }

  const NumberConversion *getConversion(NumberType from, NumberType to) {
    const NumberTypeTraits *fromTypeTraits = get(from);
    if (!fromTypeTraits)
      return nullptr;

    return fromTypeTraits->conversionTable.get(to);
  }

  static inline NumberTypeLookup &getInstance() {
    static NumberTypeLookup instance;
    return instance;
  }

private:
  void buildConversionInfo();
  void set(const NumberTypeTraits &traits);
};

struct VectorTypeTraits {
  size_t dimension = 0;
  bool isInteger;
  CBType cbType;
  chainblocks::Type type;
  NumberType numberType;
  const char *name;
};

struct VectorTypeLookup {
private:
  std::vector<const VectorTypeTraits *> dimensionLookupTables[2];
  std::vector<const VectorTypeTraits *> cbTypeLookup;
  std::vector<VectorTypeTraits> vectorTypes;

public:
  VectorTypeLookup();
  const VectorTypeTraits *get(CBType type);
  const VectorTypeTraits *findCompatibleType(bool isInteger, size_t dimension);
  static inline VectorTypeLookup &getInstance() {
    static VectorTypeLookup instance;
    return instance;
  }

private:
  void buildCBTypeLookup();
  void buildDimensionLookup();
};

}; // namespace chainblocks
