#ifndef D21D6CFA_EF1C_4BC8_A578_131D78F4D38F
#define D21D6CFA_EF1C_4BC8_A578_131D78F4D38F

#include <shards/shards.hpp>
#include <shards/iterator.hpp>
#include <shards/defer.hpp>
#include <boost/container/small_vector.hpp>
#include <string>
#include <set>
#include <spdlog/spdlog.h>
#include "ops_internal.hpp"

namespace shards {
struct TypeMatcherErrorFormatter {
  static constexpr std::true_type type_matcher_error_interface{};

  boost::container::small_vector<std::string, 8> path;
  std::vector<std::string> errors;

  void formatPath(std::string &output) {
    if (path.empty()) {
      return;
    }

    output += path[0];
    for (size_t i = 1; i < path.size(); i++) {
      output += ":";
      output += path[i];
    }
  }
};

template <typename T>
concept ErrorFormatterInterface = requires(T t, std::string outStr) {
  { t.errors } -> std::same_as<std::vector<std::string> &>;
  { t.path } -> std::same_as<boost::container::small_vector<std::string, 8> &>;
  { t.formatPath(outStr) };
};

template <typename ErrorFormatter = std::monostate> struct TypeMatcher {
  // This will automatically allow extra keys in the input table
  // e.g. Input type {a: Int b: Float} will match against receiver type {a: Int}
  bool isParameter = true;
  bool strict = true;
  bool relaxEmptyTableCheck = true;
  bool relaxEmptySeqCheck = false;
  bool checkVarTypes = false;
  bool ignoreFixedSeq = false;

  TypeMatcherErrorFormatter errorFormatter;

  void logTypeMismatch(const SHTypeInfo &expected, const SHTypesInfo &actual) {
    if constexpr (ErrorFormatterInterface<ErrorFormatter>) {
      std::string err;
      errorFormatter.formatPath(err);
      if (!err.empty())
        err += ", ";
      fmt::format_to(std::back_inserter(err), "type mismatch, was: {}, expected: {}", expected, actual);
      errorFormatter.errors.emplace_back(std::move(err));
    }
  }

  void logTypeMismatch(const SHTypeInfo &expected, const SHTypeInfo &actual) {
    if constexpr (ErrorFormatterInterface<ErrorFormatter>) {
      std::string err;
      errorFormatter.formatPath(err);
      if (!err.empty())
        err += ", ";
      fmt::format_to(std::back_inserter(err), "type mismatch, was: {}, expected: {}", expected, actual);
      errorFormatter.errors.emplace_back(std::move(err));
    }
  }

  void logKeysMissing(const std::set<SHVar> &keys) {
    if constexpr (ErrorFormatterInterface<ErrorFormatter>) {
      std::string err;
      errorFormatter.formatPath(err);
      if (!err.empty())
        err += ", ";

      fmt::format_to(std::back_inserter(err), "input is missing keys: ");
      size_t nk = 0;
      for (auto &k : keys) {
        if (nk > 0)
          fmt::format_to(std::back_inserter(err), ", ");
        fmt::format_to(std::back_inserter(err), "{}", k);
        nk++;
      }
      errorFormatter.errors.emplace_back(std::move(err));
    }
  }

  void appendPath(std::string_view path) {
    if constexpr (ErrorFormatterInterface<ErrorFormatter>) {
      errorFormatter.path.emplace_back(path);
    }
  }
  void appendPath(const char *path) {
    if constexpr (ErrorFormatterInterface<ErrorFormatter>) {
      appendPath(std::string_view(path));
    }
  }
  void appendPath(const SHVar &v) {
    if constexpr (ErrorFormatterInterface<ErrorFormatter>) {
      appendPath(fmt::format("{}", v));
    }
  }
  void popPath() {
    if constexpr (ErrorFormatterInterface<ErrorFormatter>) {
      errorFormatter.path.pop_back();
    }
  }

  bool match(const SHTypeInfo &inputType, const SHTypeInfo &receiverType) {
    if (!matchInner(inputType, receiverType)) {
      logTypeMismatch(inputType, receiverType);
      return false;
    }
    return true;
  }

  bool matchInner(const SHTypeInfo &inputType, const SHTypeInfo &receiverType) {
    if (receiverType.basicType == SHType::Any)
      return true;

    if (inputType.basicType != receiverType.basicType) {
      // Fail if basic type differs
      return false;
    }

    switch (inputType.basicType) {
    case SHType::Object: {
      if (inputType.object.vendorId != receiverType.object.vendorId || inputType.object.typeId != receiverType.object.typeId) {
        return false;
      }
      if (receiverType.object.extInfo && receiverType.object.extInfo->match) {
        shassert(receiverType.object.extInfo == inputType.object.extInfo);
        if (!receiverType.object.extInfo->match(receiverType.object.extInfoData, inputType.object.extInfoData)) {
          return false;
        }
      }
      break;
    }
    case SHType::Enum: {
      // special case: any enum
      if (receiverType.enumeration.vendorId == 0 && receiverType.enumeration.typeId == 0)
        return true;
      // otherwise, exact match
      if (inputType.enumeration.vendorId != receiverType.enumeration.vendorId ||
          inputType.enumeration.typeId != receiverType.enumeration.typeId) {
        return false;
      }
      break;
    }
    case SHType::Seq: {
      if (strict) {
        if (inputType.seqTypes.len == 0 && receiverType.seqTypes.len == 0) {
          return true;
        } else if (inputType.seqTypes.len > 0 && receiverType.seqTypes.len > 0) {
          // all input types must be in receiver, receiver can have more ofc
          for (uint32_t i = 0; i < inputType.seqTypes.len; i++) {
            for (uint32_t j = 0; j < receiverType.seqTypes.len; j++) {
              if (receiverType.seqTypes.elements[j].basicType == SHType::Any ||
                  match(inputType.seqTypes.elements[i], receiverType.seqTypes.elements[j]))
                goto matched;
            }
            logTypeMismatch(inputType.seqTypes.elements[i], receiverType.seqTypes);
            return false;
          matched:
            continue;
          }
        } else if (inputType.seqTypes.len == 0 && receiverType.seqTypes.len > 0 && !relaxEmptySeqCheck) {
          // Empty input sequence type indicates [ Any ], receiver type needs to explicitly contain Any to match
          // but if input is a parameter such as `[]` we can let it pass, this is also used in channels!
          for (uint32_t j = 0; j < receiverType.seqTypes.len; j++) {
            if (receiverType.seqTypes.elements[j].basicType == SHType::Any)
              return true;
          }
          return false;
        } else if ((!relaxEmptySeqCheck && inputType.seqTypes.len == 0) || receiverType.seqTypes.len == 0) {
          return false;
        }
        // if a fixed size is requested make sure it fits at least enough elements
        if (!ignoreFixedSeq && receiverType.fixedSize != 0 && inputType.fixedSize != 0 && // for now check only if both fixed
            receiverType.fixedSize > inputType.fixedSize) {
          return false;
        }
      }
      break;
    }
    case SHType::Table: {
      if (strict) {
        // Table is a complicated one
        // We use it as many things.. one of it as structured data
        // So we have many possible cases:
        // 1. a receiver table with just type info is flexible, accepts only those
        // types but the keys are open to anything, if no types are available, it
        // accepts any type
        // 2. a receiver table with type info and key info is strict, means that
        // input has to match 1:1, an exception is done if the last key is empty
        // as in
        // "" on the receiver side, in such case any input is allowed (types are
        // still checked)
        const auto numInputTypes = inputType.table.types.len;
        const auto numReceiverTypes = receiverType.table.types.len;
        const auto numInputKeys = inputType.table.keys.len;
        const auto numReceiverKeys = receiverType.table.keys.len;

        // Unkeyed receiver table case
        if (numReceiverKeys == 0) {
          // When the input is and empty table {}, and the received has no key constraints
          // pass
          if (numInputKeys == 0 && relaxEmptyTableCheck)
            return true;
          // case 1, consumer is not strict, match types if avail
          // ignore input keys information
          if (numInputTypes == 0) {
            // assume this as an Any
            if (numReceiverTypes == 0)
              return true; // both Any
            auto matched = false;
            SHTypeInfo anyType{SHType::Any};
            appendPath("<any>");
            DEFER({ popPath(); });
            for (uint32_t y = 0; y < numReceiverTypes; y++) {
              auto btype = receiverType.table.types.elements[y];
              if (match(anyType, btype)) {
                matched = true;
                break;
              }
            }
            if (!matched) {
              return false;
            }
          } else {
            if (isParameter || numReceiverTypes != 0) {
              // receiver doesn't accept anything, match further
              for (uint32_t i = 0; i < numInputTypes; i++) {
                // Go thru all exposed types and make sure we get a positive match
                // with the consumer
                auto atype = inputType.table.types.elements[i];
                auto matched = false;
                appendPath("<any>");
                DEFER({ popPath(); });
                for (uint32_t y = 0; y < numReceiverTypes; y++) {
                  auto btype = receiverType.table.types.elements[y];
                  if (match(atype, btype)) {
                    matched = true;
                    break;
                  }
                }
                if (!matched) {
                  logTypeMismatch(atype, receiverType.table.types);
                  return false;
                }
              }
            }
          }
        } else {
          // Keyed receiver table case
          // Last element being empty ("") indicates that keys not in the type can match
          // in that case they will be matched against the last type element at the same position
          const auto lastElementEmpty = receiverType.table.keys.elements[numReceiverKeys - 1].valueType == SHType::None;

          bool ignoreExtra = isParameter;

          // If we need a 1:1 match in this case, fail early
          if (!lastElementEmpty && !ignoreExtra && (numInputKeys != numReceiverKeys || numInputKeys != numInputTypes)) {
            return false;
          }

          std::set<SHVar> missingReceiverKeys{};
          if constexpr (ErrorFormatterInterface<ErrorFormatter>) {
            for (uint32_t i = 0; i < numReceiverKeys; i++) {
              missingReceiverKeys.insert(receiverType.table.keys.elements[i]);
            }
          }

          auto missingRecvMatches = lastElementEmpty ? numReceiverKeys - 1 : numReceiverKeys;
          for (uint32_t i = 0; i < numInputKeys; i++) {
            auto inputEntryType = inputType.table.types.elements[i];
            auto inputEntryKey = inputType.table.keys.elements[i];
            for (uint32_t y = 0; y < numReceiverKeys; y++) {
              auto receiverEntryType = receiverType.table.types.elements[y];
              auto receiverEntryKey = receiverType.table.keys.elements[y];
              // Try to compare against the wildcard type first
              if (lastElementEmpty && y == (numReceiverKeys - 1)) {
                appendPath("<any>");
                DEFER({ popPath(); });

                if (match(inputEntryType, receiverEntryType)) {
                  y = numReceiverKeys; // break
                } else {
                  logTypeMismatch(inputEntryType, receiverEntryType);
                  return false;
                }
              } else if (inputEntryKey == receiverEntryKey) {
                appendPath(inputEntryKey);
                DEFER({ popPath(); });

                if (match(inputEntryType, receiverEntryType)) {
                  missingRecvMatches--;
                  if constexpr (ErrorFormatterInterface<ErrorFormatter>) {
                    missingReceiverKeys.erase(receiverEntryKey);
                  }
                  y = numReceiverKeys; // break
                } else {
                  logTypeMismatch(inputEntryType, receiverEntryType);
                  return false;
                }
              }
            }
          }

          if (missingRecvMatches) {
            logKeysMissing(missingReceiverKeys);
            return false;
          }
        }
      }
      break;
    }
    case SHType::ContextVar: {
      if (!checkVarTypes)
        break;

      for (auto &innerReceiverType : receiverType.contextVarTypes) {
        bool matchAll = true;
        for (auto &t : inputType.contextVarTypes) {
          if (!match(t, innerReceiverType)) {
            matchAll = false;
            break;
          }
        }
        if (matchAll) {
          return true;
        }
      }
      return false;
    }
    default:
      return true;
    }
    return true;
  }
};
} // namespace shards

#endif /* D21D6CFA_EF1C_4BC8_A578_131D78F4D38F */
