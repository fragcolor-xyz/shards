#ifndef D21D6CFA_EF1C_4BC8_A578_131D78F4D38F
#define D21D6CFA_EF1C_4BC8_A578_131D78F4D38F

#include <shards/shards.hpp>
#include <shards/iterator.hpp>

namespace shards {
struct TypeMatcher {
  bool isParameter = true;
  bool strict = true;
  bool relaxEmptyTableCheck = true;
  bool relaxEmptySeqCheck = false;
  bool checkVarTypes = false;
  bool ignoreFixedSeq = false;

  bool match(const SHTypeInfo &inputType, const SHTypeInfo &receiverType) {
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
      if (receiverType.object.extended) {
        if (!receiverType.object.extended->matchType(&receiverType, &inputType)) {
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
        if (!ignoreFixedSeq && receiverType.fixedSize != 0 && receiverType.fixedSize > inputType.fixedSize) {
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

        // When the input is and empty table {}, and the received has no key constraints
        // pass
        if (numReceiverKeys == 0 && numInputKeys == 0 && relaxEmptyTableCheck) {
          return true;
        } else if (numReceiverKeys == 0) {
          // case 1, consumer is not strict, match types if avail
          // ignore input keys information
          if (numInputTypes == 0) {
            // assume this as an Any
            if (numReceiverTypes == 0)
              return true; // both Any
            auto matched = false;
            SHTypeInfo anyType{SHType::Any};
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
                for (uint32_t y = 0; y < numReceiverTypes; y++) {
                  auto btype = receiverType.table.types.elements[y];
                  if (match(atype, btype)) {
                    matched = true;
                    break;
                  }
                }
                if (!matched) {
                  return false;
                }
              }
            }
          }
        } else {
          if (!isParameter && numInputKeys == 0 && numInputTypes == 0)
            return true; // update case {} >= .edit-me {"x" 10} > .edit-me

          // Last element being empty ("") indicates that keys not in the type can match
          // in that case they will be matched against the last type element at the same position
          const auto lastElementEmpty = receiverType.table.keys.elements[numReceiverKeys - 1].valueType == SHType::None;
          if (!lastElementEmpty && (numInputKeys != numReceiverKeys || numInputKeys != numInputTypes)) {
            // we need a 1:1 match in this case, fail early
            return false;
          }

          auto missingMatches = numInputKeys;
          for (uint32_t i = 0; i < numInputKeys; i++) {
            auto inputEntryType = inputType.table.types.elements[i];
            auto inputEntryKey = inputType.table.keys.elements[i];
            for (uint32_t y = 0; y < numReceiverKeys; y++) {
              auto receiverEntryType = receiverType.table.types.elements[y];
              auto receiverEntryKey = receiverType.table.keys.elements[y];
              // Either match the expected key's type or compare against the last type (if it's key is "")
              if (inputEntryKey == receiverEntryKey || (lastElementEmpty && y == (numReceiverKeys - 1))) {
                if (match(inputEntryType, receiverEntryType)) {
                  missingMatches--;
                  y = numReceiverKeys; // break
                } else
                  return false; // fail quick in this case
              }
            }
          }

          if (missingMatches)
            return false;
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
