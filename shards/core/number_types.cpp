#include <shards/number_types.hpp>
#include <algorithm>
#include <cstdlib>
#include <type_traits>
#include <stdexcept>

namespace shards {

std::map<SHType, NumberType> getSHTypeToNumberTypeMap() {
  // clang-format off
  return {
    {SHType::Bool, NumberType::Bool},
    {SHType::Color, NumberType::UInt8},
    {SHType::Int, NumberType::Int64},
    {SHType::Int2, NumberType::Int64},
    {SHType::Int3, NumberType::Int32},
    {SHType::Int4, NumberType::Int32},
    {SHType::Int8, NumberType::Int16},
    {SHType::Int16, NumberType::Int8},
    {SHType::Float, NumberType::Float64},
    {SHType::Float2, NumberType::Float64},
    {SHType::Float3, NumberType::Float32},
    {SHType::Float4, NumberType::Float32},
  };
  // clang-format on
};

const Type &getCompatibleUnitType(NumberType numberType) {
  switch (numberType) {
  default:
    throw std::logic_error("Invalid number type");
  case NumberType::UInt8:
  case NumberType::Int8:
  case NumberType::Int16:
  case NumberType::Int32:
  case NumberType::Int64:
    return CoreInfo::IntType;
    break;
  case NumberType::Float32:
  case NumberType::Float64:
    return CoreInfo::FloatType;
  }
}

template <typename TIn, typename TOut> struct TNumberConversion : NumberConversion {
  TNumberConversion() {
    inStride = sizeof(TIn);
    outStride = sizeof(TOut);
    convertOne = [](const void *src, void *dst) { ((TOut *)dst)[0] = (TOut)((TIn *)src)[0]; };
    convertMultipleSeq = [](const void *src, void *dst, size_t srcLen, const SHSeq &sequence) {
      for (size_t dstIndex = 0; dstIndex < sequence.len; dstIndex++) {
        SHInt srcIndex = sequence.elements[dstIndex].payload.intValue;
        if (srcIndex >= (SHInt)srcLen || srcIndex < 0) {
          throw NumberConversionOutOfRangeEx(srcIndex);
        }

        ((TOut *)dst)[dstIndex] = (TOut)((TIn *)src)[srcIndex];
      }
    };
  }
};

template <typename TIn> struct TNumberConversionTable : public NumberConversionTable {
  TNumberConversionTable() {
    set(NumberType::Bool, TNumberConversion<TIn, bool>());
    set(NumberType::UInt8, TNumberConversion<TIn, uint8_t>());
    set(NumberType::Int8, TNumberConversion<TIn, int8_t>());
    set(NumberType::Int16, TNumberConversion<TIn, int16_t>());
    set(NumberType::Int32, TNumberConversion<TIn, int32_t>());
    set(NumberType::Int64, TNumberConversion<TIn, int64_t>());
    set(NumberType::Float32, TNumberConversion<TIn, float>());
    set(NumberType::Float64, TNumberConversion<TIn, double>());
  }

  void set(const NumberType &numberType, const NumberConversion &numberConversion) {
    conversions.resize(std::max(conversions.size(), size_t(numberType) + 1));
    conversions[size_t(numberType)] = numberConversion;
  }
};

template <typename TIn> struct TNumberStringOperations {};

template <> struct TNumberStringOperations<uint8_t> {
  static void parse(uint8_t *out, const char *input, char **inputEnd) { out[0] = (uint8_t)std::strtoul(input, inputEnd, 10); }
};

template <> struct TNumberStringOperations<int8_t> {
  static void parse(int8_t *out, const char *input, char **inputEnd) { out[0] = (int8_t)std::strtol(input, inputEnd, 10); }
};

template <> struct TNumberStringOperations<int16_t> {
  static void parse(int16_t *out, const char *input, char **inputEnd) { out[0] = (int16_t)std::strtoul(input, inputEnd, 10); }
};

template <> struct TNumberStringOperations<int32_t> {
  static void parse(int32_t *out, const char *input, char **inputEnd) { out[0] = std::strtol(input, inputEnd, 10); }
};

template <> struct TNumberStringOperations<int64_t> {
  static void parse(int64_t *out, const char *input, char **inputEnd) { out[0] = std::strtoll(input, inputEnd, 10); }
};

template <> struct TNumberStringOperations<float> {
  static void parse(float *out, const char *input, char **inputEnd) { out[0] = std::strtof(input, inputEnd); }
};

template <> struct TNumberStringOperations<double> {
  static void parse(double *out, const char *input, char **inputEnd) { out[0] = std::strtod(input, inputEnd); }
};

template <> struct TNumberStringOperations<bool> {
  static void parse(double *out, const char *input, char **inputEnd) {
    std::string_view sv(input);
    out[0] = sv == "true" || sv == "True" || sv == "TRUE";
  }
};

template <NumberType Type> struct TNumberTypeTraits {};

#define NUMBER_TYPE_TRAITS(_Type, _CType)                                         \
  template <> struct TNumberTypeTraits<_Type> : public NumberTypeTraits {         \
    typedef _CType TInner;                                                        \
    TNumberTypeTraits() {                                                         \
      type = _Type;                                                               \
      isInteger = std::is_integral<_CType>::value;                                \
      size = sizeof(_CType);                                                      \
      conversionTable = TNumberConversionTable<_CType>();                         \
      convertParse = (NumberConvertParse)&TNumberStringOperations<_CType>::parse; \
    }                                                                             \
  };

NUMBER_TYPE_TRAITS(NumberType::Bool, bool);
NUMBER_TYPE_TRAITS(NumberType::UInt8, uint8_t);
NUMBER_TYPE_TRAITS(NumberType::Int8, int8_t);
NUMBER_TYPE_TRAITS(NumberType::Int16, int16_t);
NUMBER_TYPE_TRAITS(NumberType::Int32, int32_t);
NUMBER_TYPE_TRAITS(NumberType::Int64, int64_t);
NUMBER_TYPE_TRAITS(NumberType::Float32, float);
NUMBER_TYPE_TRAITS(NumberType::Float64, double);

#undef NUMBER_TYPE_TRAITS

template <SHType Type> struct TVectorTypeTraits {};

#define EXPAND(_x) _x
#define STRINGIFY(_x) EXPAND(#_x)
#define VECTOR_TYPE_TRAITS(_SHType, _Dimension, _NumberType)                        \
  template <> struct TVectorTypeTraits<SHType::_SHType> : public VectorTypeTraits { \
    typedef TNumberTypeTraits<_NumberType>::TInner TInner;                          \
    TVectorTypeTraits() {                                                           \
      dimension = _Dimension;                                                       \
      isInteger = std::is_integral<TInner>::value;                                  \
      shType = SHType::_SHType;                                                     \
      type = shards::CoreInfo::_SHType##Type;                                       \
      numberType = _NumberType;                                                     \
      name = STRINGIFY(_SHType);                                                    \
    }                                                                               \
  };

VECTOR_TYPE_TRAITS(Bool, 1, NumberType::Bool);
VECTOR_TYPE_TRAITS(Color, 4, NumberType::UInt8);
VECTOR_TYPE_TRAITS(Int, 1, NumberType::Int64);
VECTOR_TYPE_TRAITS(Int2, 2, NumberType::Int64);
VECTOR_TYPE_TRAITS(Int3, 3, NumberType::Int32);
VECTOR_TYPE_TRAITS(Int4, 4, NumberType::Int32);
VECTOR_TYPE_TRAITS(Int8, 8, NumberType::Int16);
VECTOR_TYPE_TRAITS(Int16, 16, NumberType::Int8);
VECTOR_TYPE_TRAITS(Float, 1, NumberType::Float64);
VECTOR_TYPE_TRAITS(Float2, 2, NumberType::Float64);
VECTOR_TYPE_TRAITS(Float3, 3, NumberType::Float32);
VECTOR_TYPE_TRAITS(Float4, 4, NumberType::Float32);

#undef VECTOR_TYPE_TRAITS
#undef STRINGIFY
#undef EXPAND

VectorTypeLookup::VectorTypeLookup() {
  // clang-format off
    vectorTypes = {
        TVectorTypeTraits<SHType::Bool>(),
        TVectorTypeTraits<SHType::Color>(),
        TVectorTypeTraits<SHType::Int>(),
        TVectorTypeTraits<SHType::Int2>(),
        TVectorTypeTraits<SHType::Int3>(),
        TVectorTypeTraits<SHType::Int4>(),
        TVectorTypeTraits<SHType::Int8>(),
        TVectorTypeTraits<SHType::Int16>(),
        TVectorTypeTraits<SHType::Float>(),
        TVectorTypeTraits<SHType::Float2>(),
        TVectorTypeTraits<SHType::Float3>(),
        TVectorTypeTraits<SHType::Float4>(),
    };
  // clang-format on
  buildSHTypeLookup();
  buildDimensionLookup();
}

NumberTypeLookup::NumberTypeLookup() {
  set(TNumberTypeTraits<NumberType::UInt8>());
  set(TNumberTypeTraits<NumberType::Int8>());
  set(TNumberTypeTraits<NumberType::Int16>());
  set(TNumberTypeTraits<NumberType::Int32>());
  set(TNumberTypeTraits<NumberType::Int64>());
  set(TNumberTypeTraits<NumberType::Float32>());
  set(TNumberTypeTraits<NumberType::Float64>());
  buildConversionInfo();
}

void NumberTypeLookup::set(const NumberTypeTraits &traits) {
  numberTypes.resize(std::max(numberTypes.size(), size_t(traits.type) + 1));
  numberTypes[size_t(traits.type)] = traits;
}

void NumberTypeLookup::buildConversionInfo() {
  auto shTypeToNumberTypeMap = getSHTypeToNumberTypeMap();

  for (const auto &it : shTypeToNumberTypeMap) {
    size_t shTypeIndex = (size_t)it.first;
    shTypeLookup.resize(std::max(shTypeLookup.size(), shTypeIndex + 1));
    shTypeLookup[shTypeIndex] = get(it.second);
  }
}

void VectorTypeLookup::buildSHTypeLookup() {
  for (const VectorTypeTraits &vectorType : vectorTypes) {
    shTypeLookup.resize(std::max(shTypeLookup.size(), size_t(vectorType.shType) + 1));
    shTypeLookup[size_t(vectorType.shType)] = &vectorType;
  }
}

void VectorTypeLookup::buildDimensionLookup() {
  std::vector<const VectorTypeTraits *> sortedVectorTypes;
  for (const VectorTypeTraits &vectorType : vectorTypes) {
    sortedVectorTypes.push_back(&vectorType);
  }
  std::sort(sortedVectorTypes.begin(), sortedVectorTypes.end(),
            [](const VectorTypeTraits *a, const VectorTypeTraits *b) { return a->dimension < b->dimension; });

  size_t tails[2] = {0};
  for (const VectorTypeTraits *vectorType : sortedVectorTypes) {
    // Quick filter to ignore bools in compatible type map
    if (vectorType->shType == SHType::Bool)
      continue;

    size_t typeIndex = vectorType->isInteger ? 0 : 1;
    std::vector<const VectorTypeTraits *> &compatibleVectorTypeMap = dimensionLookupTables[typeIndex];

    compatibleVectorTypeMap.resize(vectorType->dimension + 1);
    for (size_t i = tails[typeIndex]; i <= vectorType->dimension; i++) {
      compatibleVectorTypeMap[i] = vectorType;
    }
    tails[typeIndex] = vectorType->dimension + 1;
  }
}

const VectorTypeTraits *VectorTypeLookup::get(SHType type) {
  if (size_t(type) >= shTypeLookup.size())
    return nullptr;

  return shTypeLookup[size_t(type)];
}

const VectorTypeTraits *VectorTypeLookup::findCompatibleType(bool isInteger, size_t dimension) {
  size_t typeIndex = isInteger ? 0 : 1;
  std::vector<const VectorTypeTraits *> &compatibleVectorTypeMap = dimensionLookupTables[typeIndex];

  for (; dimension < compatibleVectorTypeMap.size(); ++dimension) {
    const VectorTypeTraits *compatibleType = compatibleVectorTypeMap[dimension];
    if (compatibleType)
      return compatibleType;
  }

  return nullptr;
}

const std::vector<NumberType> &getSHTypeToNumberTypeArrayMap() {
  static std::vector<NumberType> result = []() {
    std::vector<NumberType> result;
    auto map = getSHTypeToNumberTypeMap();
    for (auto &it : map) {
      size_t shTypeIndex = size_t(it.first);
      result.resize(std::max(result.size(), shTypeIndex + 1));
      result[shTypeIndex] = it.second;
    }
    return result;
  }();
  return result;
}

}; // namespace shards
