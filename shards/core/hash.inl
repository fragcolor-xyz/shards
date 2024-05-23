#ifndef A5205C99_19A3_41F5_8B1D_E6CAAFC16BE9
#define A5205C99_19A3_41F5_8B1D_E6CAAFC16BE9

#include "hash.hpp"

namespace shards {
// this is potentially called from unsafe code (e.g. networking)
// let's do some stack protection here
constexpr int MAX_DERIVED_TYPE_HASH_RECURSION = 100;

template <typename TDigest> void hashReset(XXH3_state_t *state) {
  if constexpr (sizeof(TDigest) == 8) {
    XXH3_64bits_reset(state);
  } else {
    XXH3_128bits_reset(state);
  }
}
template <typename TDigest> TDigest hashDigest(XXH3_state_t *state) {
  if constexpr (sizeof(TDigest) == 8) {
    return XXH3_64bits_digest(state);
  } else {
    return XXH3_128bits_digest(state);
  }
}
template <typename TDigest> XXH_errorcode hashUpdate(XXH3_state_t *state, const void *input, size_t len) {
  if constexpr (sizeof(TDigest) == 8) {
    return XXH3_64bits_update(state, input, len);
  } else {
    return XXH3_128bits_update(state, input, len);
  }
}

#define PUSH_TMP_HASH_SET()                 \
  int hashSetIndex = hashSetCounter++;      \
  DEFER({ --hashSetCounter; });             \
  if (hashSetIndex >= (int)hashSets.size()) \
    hashSets.resize(hashSetIndex + 1);      \
  hashSets[hashSetIndex].clear();

#define TMP_HASH_SET (hashSets[hashSetIndex])

template <typename TDigest> inline void HashState<TDigest>::updateTypeHash(const SHVar &var, XXH3_state_s *state) {
  DEFER({ --depth; });
  if ((++depth) >= MAX_DERIVED_TYPE_HASH_RECURSION)
    throw SHException("HashState maximum recursion exceeded");

  // this is not complete at all, missing Array and SHType::ContextVar for example
  hashUpdate<TDigest>(state, &var.valueType, sizeof(var.valueType));
  hashUpdate<TDigest>(state, &var.innerType, sizeof(var.innerType));

  switch (var.valueType) {
  case SHType::Object: {
    hashUpdate<TDigest>(state, &var.payload.objectVendorId, sizeof(var.payload.objectVendorId));
    hashUpdate<TDigest>(state, &var.payload.objectTypeId, sizeof(var.payload.objectTypeId));
    break;
  }
  case SHType::Enum: {
    hashUpdate<TDigest>(state, &var.payload.enumVendorId, sizeof(var.payload.enumVendorId));
    hashUpdate<TDigest>(state, &var.payload.enumTypeId, sizeof(var.payload.enumTypeId));
    break;
  }
  case SHType::Seq: {
    // Needed to sort hashes
    PUSH_TMP_HASH_SET();

    for (uint32_t i = 0; i < var.payload.seqValue.len; i++) {
      auto typeHash = deriveTypeHash(var.payload.seqValue.elements[i]);
      TMP_HASH_SET.insert(typeHash);
    }
    constexpr auto recursive = false;
    hashUpdate<TDigest>(state, &recursive, sizeof(recursive));
    for (const auto &hash : TMP_HASH_SET) {
      hashUpdate<TDigest>(state, &hash, sizeof(TDigest));
    }
  } break;
  case SHType::Table: {
    // Needed to sort hashes
    PUSH_TMP_HASH_SET();

    auto &t = var.payload.tableValue;
    SHTableIterator tit;
    t.api->tableGetIterator(t, &tit);
    SHVar k;
    SHVar v;
    while (t.api->tableNext(t, &tit, &k, &v)) {
      XXH3_state_t subState{};
      hashReset<TDigest>(&subState);
      updateTypeHash(k, &subState);
      updateTypeHash(v, &subState);
      TMP_HASH_SET.insert(hashDigest<TDigest>(&subState));
    }

    for (const auto hash : TMP_HASH_SET) {
      hashUpdate<TDigest>(state, &hash, sizeof(hash));
    }

  } break;
  case SHType::Set: {
    // Needed to sort hashes
    PUSH_TMP_HASH_SET();

    // set is unordered so just collect
    auto &s = var.payload.setValue;
    SHSetIterator sit;
    s.api->setGetIterator(s, &sit);
    SHVar v;
    while (s.api->setNext(s, &sit, &v)) {
      TMP_HASH_SET.insert(deriveTypeHash(v));
    }
    constexpr auto recursive = false;
    hashUpdate<TDigest>(state, &recursive, sizeof(recursive));
    for (const TDigest hash : TMP_HASH_SET) {
      hashUpdate<TDigest>(state, &hash, sizeof(TDigest));
    }
  } break;
  default:
    break;
  };
}

template <typename TDigest> inline void HashState<TDigest>::updateTypeHash(const SHTypeInfo &t, XXH3_state_s *state) {
  DEFER({ --depth; });
  if ((++depth) >= MAX_DERIVED_TYPE_HASH_RECURSION)
    throw SHException("HashState maximum recursion exceeded");

  hashUpdate<TDigest>(state, &t.basicType, sizeof(t.basicType));
  hashUpdate<TDigest>(state, &t.innerType, sizeof(t.innerType));

  switch (t.basicType) {
  case SHType::Object: {
    hashUpdate<TDigest>(state, &t.object.vendorId, sizeof(t.object.vendorId));
    hashUpdate<TDigest>(state, &t.object.typeId, sizeof(t.object.typeId));
    if (t.object.extInfo && t.object.extInfo->hash) {
      TDigest digest{};
      t.object.extInfo->hash(t.object.extInfoData, &digest, sizeof(TDigest));
      hashUpdate<TDigest>(state, &digest, sizeof(TDigest));
    }
  } break;
  case SHType::Enum: {
    hashUpdate<TDigest>(state, &t.enumeration.vendorId, sizeof(t.enumeration.vendorId));
    hashUpdate<TDigest>(state, &t.enumeration.typeId, sizeof(t.enumeration.typeId));
  } break;
  case SHType::Seq: {
    // Needed to sort hashes
    PUSH_TMP_HASH_SET();

    bool recursive = false;

    for (uint32_t i = 0; i < t.seqTypes.len; i++) {
      if (t.seqTypes.elements[i].recursiveSelf) {
        recursive = true;
      } else {
        auto typeHash = deriveTypeHash(t.seqTypes.elements[i]);
        TMP_HASH_SET.insert(typeHash);
      }
    }
    hashUpdate<TDigest>(state, &recursive, sizeof(recursive));
    for (const auto hash : TMP_HASH_SET) {
      hashUpdate<TDigest>(state, &hash, sizeof(hash));
    }
  } break;
  case SHType::Table: {
    if (t.table.keys.len == t.table.types.len) {
      // Needed to sort hashes
      PUSH_TMP_HASH_SET();

      for (uint32_t i = 0; i < t.table.types.len; i++) {
        XXH3_state_t subState{};
        hashReset<TDigest>(&subState);
        updateTypeHash(t.table.keys.elements[i], &subState);
        updateTypeHash(t.table.types.elements[i], &subState);
        TMP_HASH_SET.insert(hashDigest<TDigest>(&subState));
      }

      for (const auto hash : TMP_HASH_SET) {
        hashUpdate<TDigest>(state, &hash, sizeof(hash));
      }
    } else {
      // Needed to sort hashes
      PUSH_TMP_HASH_SET();

      for (uint32_t i = 0; i < t.table.types.len; i++) {
        auto typeHash = deriveTypeHash(t.table.types.elements[i]);
        TMP_HASH_SET.insert(typeHash);
      }
      for (const auto hash : TMP_HASH_SET) {
        hashUpdate<TDigest>(state, &hash, sizeof(hash));
      }
    }
  } break;
  case SHType::Set: {
    // Needed to sort hashes
    PUSH_TMP_HASH_SET();

    bool recursive = false;
    for (uint32_t i = 0; i < t.setTypes.len; i++) {
      if (t.setTypes.elements[i].recursiveSelf) {
        recursive = true;
      } else {
        auto typeHash = deriveTypeHash(t.setTypes.elements[i]);
        TMP_HASH_SET.insert(typeHash);
      }
    }
    hashUpdate<TDigest>(state, &recursive, sizeof(recursive));
    for (const auto hash : TMP_HASH_SET) {
      hashUpdate<TDigest>(state, &hash, sizeof(hash));
    }
  } break;
  default:
    break;
  };
}
template <typename TDigest> inline void HashState<TDigest>::updateHash(const SHVar &var, XXH3_state_s *state) {
  auto error = hashUpdate<TDigest>(state, &var.valueType, sizeof(var.valueType));
  shassert(error == XXH_OK);

  switch (var.valueType) {
  case SHType::Type: {
    updateTypeHash(*var.payload.typeValue, state);
  } break;
  case SHType::Bytes: {
    error = hashUpdate<TDigest>(state, var.payload.bytesValue, size_t(var.payload.bytesSize));
    shassert(error == XXH_OK);
  } break;
  case SHType::Path:
  case SHType::ContextVar:
  case SHType::String: {
    error = hashUpdate<TDigest>(state, var.payload.stringValue,
                                size_t(var.payload.stringLen > 0 || var.payload.stringValue == nullptr
                                           ? var.payload.stringLen
                                           : strlen(var.payload.stringValue)));
    shassert(error == XXH_OK);
  } break;
  case SHType::Image: {
    SHImage i = var.payload.imageValue;
    i.data = nullptr;
    error = hashUpdate<TDigest>(state, &i, sizeof(SHImage));
    shassert(error == XXH_OK);
    auto pixsize = getPixelSize(var);
    error = hashUpdate<TDigest>(
        state, var.payload.imageValue.data,
        size_t(var.payload.imageValue.channels * var.payload.imageValue.width * var.payload.imageValue.height * pixsize));
    shassert(error == XXH_OK);
  } break;
  case SHType::Audio: {
    SHAudio a = var.payload.audioValue;
    a.samples = nullptr;
    error = hashUpdate<TDigest>(state, &a, sizeof(SHAudio));
    shassert(error == XXH_OK);
    error = hashUpdate<TDigest>(state, var.payload.audioValue.samples,
                                size_t(var.payload.audioValue.channels * var.payload.audioValue.nsamples * sizeof(float)));
    shassert(error == XXH_OK);
  } break;
  case SHType::Seq: {
    for (uint32_t i = 0; i < var.payload.seqValue.len; i++) {
      updateHash(var.payload.seqValue.elements[i], state);
    }
  } break;
  case SHType::Array: {
    for (uint32_t i = 0; i < var.payload.arrayValue.len; i++) {
      SHVar tmp{}; // only of blittable types and hash uses just type, so no init
                   // needed
      tmp.valueType = var.innerType;
      tmp.payload = var.payload.arrayValue.elements[i];
      updateHash(tmp, state);
    }
  } break;
  case SHType::Table: {
    // table is sorted, do all in 1 iteration
    auto &t = var.payload.tableValue;
    SHTableIterator it;
    t.api->tableGetIterator(t, &it);
    SHVar key;
    SHVar value;
    while (t.api->tableNext(t, &it, &key, &value)) {
      const auto kh = hash(key);
      hashUpdate<TDigest>(state, &kh, sizeof(TDigest));
      const auto h = hash(key);
      hashUpdate<TDigest>(state, &h, sizeof(TDigest));
    }
  } break;
  case SHType::Set: {
    // Needed to sort hashes
    PUSH_TMP_HASH_SET();

    // just store hashes, sort and actually combine later
    auto &s = var.payload.setValue;
    SHSetIterator it;
    s.api->setGetIterator(s, &it);
    SHVar value;
    while (s.api->setNext(s, &it, &value)) {
      const auto h = hash(value);
      TMP_HASH_SET.insert(h);
    }

    for (const auto &hash : TMP_HASH_SET) {
      hashUpdate<TDigest>(state, &hash, sizeof(TDigest));
    }
  } break;
  case SHType::ShardRef: {
    auto blk = var.payload.shardValue;
    auto name = blk->name(blk);
    auto error = hashUpdate<TDigest>(state, name, strlen(name));
    shassert(error == XXH_OK);

    auto params = blk->parameters(blk);
    for (uint32_t i = 0; i < params.len; i++) {
      auto pval = blk->getParam(blk, int(i));
      updateHash(pval, state);
    }

    if (blk->getState) {
      auto bstate = blk->getState(blk);
      updateHash(bstate, state);
    }
  } break;
  case SHType::Wire: {
    auto wire = SHWire::sharedFromRef(var.payload.wireValue);
    TDigest wireHash = hash(wire);
    hashUpdate<TDigest>(state, &wireHash, sizeof(TDigest));
  } break;
  case SHType::Object: {
    error = hashUpdate<TDigest>(state, &var.payload.objectVendorId, sizeof(var.payload.objectVendorId));
    shassert(error == XXH_OK);
    error = hashUpdate<TDigest>(state, &var.payload.objectTypeId, sizeof(var.payload.objectTypeId));
    shassert(error == XXH_OK);
    if ((var.flags & SHVAR_FLAGS_USES_OBJINFO) == SHVAR_FLAGS_USES_OBJINFO && var.objectInfo && var.objectInfo->hash) {
      // hash of the hash...
      auto objHash = var.objectInfo->hash(var.payload.objectValue);
      error = hashUpdate<TDigest>(state, &objHash, sizeof(uint64_t));
      shassert(error == XXH_OK);
    } else {
      // this will be valid only within this process memory space
      // better then nothing
      error = hashUpdate<TDigest>(state, &var.payload.objectValue, sizeof(var.payload.objectValue));
      shassert(error == XXH_OK);
    }
  } break;
  case SHType::None:
  case SHType::Any:
  case SHType::EndOfBlittableTypes:
    break;
  case SHType::Enum:
    error = hashUpdate<TDigest>(state, &var.payload, sizeof(SHEnum));
    shassert(error == XXH_OK);
    break;
  case SHType::Bool:
    error = hashUpdate<TDigest>(state, &var.payload, sizeof(SHBool));
    shassert(error == XXH_OK);
    break;
  case SHType::Int:
    error = hashUpdate<TDigest>(state, &var.payload, sizeof(SHInt));
    shassert(error == XXH_OK);
    break;
  case SHType::Int2:
    error = hashUpdate<TDigest>(state, &var.payload, sizeof(SHInt2));
    shassert(error == XXH_OK);
    break;
  case SHType::Int3:
    error = hashUpdate<TDigest>(state, &var.payload, sizeof(SHInt3));
    shassert(error == XXH_OK);
    break;
  case SHType::Int4:
    error = hashUpdate<TDigest>(state, &var.payload, sizeof(SHInt4));
    shassert(error == XXH_OK);
    break;
  case SHType::Int8:
    error = hashUpdate<TDigest>(state, &var.payload, sizeof(SHInt8));
    shassert(error == XXH_OK);
    break;
  case SHType::Int16:
    error = hashUpdate<TDigest>(state, &var.payload, sizeof(SHInt16));
    shassert(error == XXH_OK);
    break;
  case SHType::Color:
    error = hashUpdate<TDigest>(state, &var.payload, sizeof(SHColor));
    shassert(error == XXH_OK);
    break;
  case SHType::Float:
    error = hashUpdate<TDigest>(state, &var.payload, sizeof(SHFloat));
    shassert(error == XXH_OK);
    break;
  case SHType::Float2:
    error = hashUpdate<TDigest>(state, &var.payload, sizeof(SHFloat2));
    shassert(error == XXH_OK);
    break;
  case SHType::Float3:
    error = hashUpdate<TDigest>(state, &var.payload, sizeof(SHFloat3));
    shassert(error == XXH_OK);
    break;
  case SHType::Float4:
    error = hashUpdate<TDigest>(state, &var.payload, sizeof(SHFloat4));
    shassert(error == XXH_OK);
    break;
  case SHType::Trait:
    error = hashUpdate<TDigest>(state, &var.payload.traitValue->id, sizeof(SHTrait::id));
    shassert(error == XXH_OK);
    break;
  }
}

template <typename TDigest> inline TDigest HashState<TDigest>::deriveTypeHash(const SHVar &var) {
  XXH3_state_t state;
  hashReset<TDigest>(&state);
  updateTypeHash(var, &state);
  return hashDigest<TDigest>(&state);
}
template <typename TDigest> inline TDigest HashState<TDigest>::deriveTypeHash(const SHTypeInfo &var) {
  XXH3_state_t state;
  hashReset<TDigest>(&state);
  updateTypeHash(var, &state);
  return hashDigest<TDigest>(&state);
}
template <typename TDigest> inline TDigest HashState<TDigest>::hash(const SHVar &var) {
  XXH3_state_t state;
  hashReset<TDigest>(&state);
  updateHash(var, &state);
  return hashDigest<TDigest>(&state);
}
template <typename TDigest> inline TDigest HashState<TDigest>::hash(const std::shared_ptr<SHWire> &wire) {
  auto hashing = std::find_if(wireStack.begin(), wireStack.end(), [&](SHWire *w) { return w == wire.get(); });
  if (hashing != wireStack.end()) {
    // Do not hash recursive wires
    return TDigest{};
  }
  wireStack.push_back(wire.get());
  DEFER({ wireStack.pop_back(); });

  auto it = wireHashes.find(wire.get());
  if (it != wireHashes.end()) {
    return it->second;
  } else {
    XXH3_state_t state;
    hashReset<TDigest>(&state);

    auto error = hashUpdate<TDigest>(&state, wire->name.c_str(), wire->name.length());
    shassert(error == XXH_OK);

    error = hashUpdate<TDigest>(&state, &wire->looped, sizeof(wire->looped));
    shassert(error == XXH_OK);

    error = hashUpdate<TDigest>(&state, &wire->unsafe, sizeof(wire->unsafe));
    shassert(error == XXH_OK);

    error = hashUpdate<TDigest>(&state, &wire->pure, sizeof(wire->pure));
    shassert(error == XXH_OK);

    for (auto &blk : wire->shards) {
      SHVar tmp{};
      tmp.valueType = SHType::ShardRef;
      tmp.payload.shardValue = blk;
      updateHash(tmp, &state);
    }

    for (auto &wireVar : wire->getVariables()) {
      error = hashUpdate<TDigest>(&state, wireVar.first.payload.stringValue, wireVar.first.payload.stringLen);
      shassert(error == XXH_OK);
      updateHash(wireVar.second, &state);
    }

    TDigest hash = hashDigest<TDigest>(&state);
    wireHashes.insert_or_assign(wire.get(), hash);
    return hash;
  }
}
} // namespace shards

#endif /* A5205C99_19A3_41F5_8B1D_E6CAAFC16BE9 */
