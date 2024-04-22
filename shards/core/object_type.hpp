#ifndef C599F883_4CF5_416C_AC0D_AF07729F5C2B
#define C599F883_4CF5_416C_AC0D_AF07729F5C2B

#include <shards/shards.hpp>
#include <boost/tti/has_static_member_function.hpp>
#include <boost/tti/has_member_function.hpp>

namespace shards {

namespace detail {
BOOST_TTI_TRAIT_HAS_STATIC_MEMBER_FUNCTION(HasStaticMember_serialize, serialize);
BOOST_TTI_TRAIT_HAS_STATIC_MEMBER_FUNCTION(HasStaticMember_deserialize, deserialize);
BOOST_TTI_TRAIT_HAS_STATIC_MEMBER_FUNCTION(HasStaticMember_beforeDelete, beforeDelete);
BOOST_TTI_TRAIT_HAS_STATIC_MEMBER_FUNCTION(HasStaticMember_match, match);
BOOST_TTI_TRAIT_HAS_STATIC_MEMBER_FUNCTION(HasStaticMember_hash, hash);
BOOST_TTI_TRAIT_HAS_MEMBER_FUNCTION(HasMember_serialize, serialize);
BOOST_TTI_TRAIT_HAS_MEMBER_FUNCTION(HasMember_deserialize, deserialize);
BOOST_TTI_TRAIT_HAS_MEMBER_FUNCTION(HasMember_beforeDelete, beforeDelete);

template <typename T, typename V = void> struct ObjectInfo_IsThreadSafe {
  static constexpr bool value = T::IsThreadSafe;
};
template <typename T> struct ObjectInfo_IsThreadSafe<T> {
  static constexpr bool value = false;
};
template <typename T, typename V = void> struct ObjectInfo_Name {
  static constexpr const char value[] = T::name;
};
template <typename T> struct ObjectInfo_Name<T> {
  static constexpr const char value[] = NAMEOF_TYPE(T);
};
template <typename T, typename V = void> struct ObjectInfo_Type {
  static constexpr const SHTypeInfo value = T::type;
};
} // namespace detail

template <typename T> struct ObjectTypeTraits {
  static constexpr bool IsThreadSafe = true;

  static void display(SHStringPayload *outStr) {}
  static void serialize(T &obj, std::vector<uint8_t> &out) {
    if constexpr (detail::HasMember_serialize<T>::value) {
      obj.serialize(out);
    } else {
      throw std::runtime_error("Object does not have a serialize method");
    }
  }
};

template <bool Atomic> struct RefCount1 {
  uint32_t count;

  void reference() { ++count; }
  bool release() {
    --count;
    return count == 0;
  }
};

template <> struct RefCount1<true> {
  std::atomic<uint32_t> count;

  void reference() { ++count; }
  bool release() { return count.fetch_sub(1) == 1; }
};

template <class SH_CORE, typename T, typename Traits = ObjectTypeTraits<T>> class TObjectType {
private:
  static constexpr bool IsThreadSafe = detail::ObjectInfo_IsThreadSafe<Traits>::value;
  static inline const char *Name = detail::ObjectInfo_Name<T>::value;

  SHObjectInfo info;
  int32_t vendorId;
  int32_t typeId;

  struct ObjectRef {
    T shared;
    RefCount<detail::ObjectInfo_IsThreadSafe<Traits>::value> refcount;
  };

public:
  static inline const SHObjectInfo *getInfo() {
    static SHObjectInfo _ = []() {
      SHObjectInfo info{};
      info.name = Name;
      info.isThreadSafe = IsThreadSafe;
      if constexpr (detail::HasMember_serialize<T, void(T &, std::vector<uint8_t> &)>::value) {
        info.serialize = [](SHPointer obj, uint8_t **outData, uint64_t *outLen, SHPointer *customHandle) {
          auto tobj = reinterpret_cast<T *>(obj);
          auto holder = new std::vector<uint8_t>();
          tobj->serialize(*tobj, *holder);
          *customHandle = holder;
          *outData = holder->data();
          *outLen = holder->size();
          return true;
        };
      }
      if constexpr (detail::HasMember_deserialize<T, bool(const uint8_t *, uint64_t)>::value) {
        info.deserialize = [](uint8_t *data, uint64_t len) -> void * {
          auto r = new ObjectRef();
          if (!r->shared.deserialize((uint8_t *)data, len)) {
            delete r;
            return nullptr;
          }
          r->refcount.count = 1;
          return (SHPointer)r;
        };
      }
      info.free = [](SHPointer handle) {
        auto holder = reinterpret_cast<std::vector<uint8_t> *>(handle);
        delete holder;
      };
      info.reference = [](SHPointer ptr) {
        auto p = reinterpret_cast<ObjectRef *>(ptr);
        p->refcount.reference();
      };
      info.release = [](SHPointer ptr) {
        auto p = reinterpret_cast<ObjectRef *>(ptr);
        if (p->refcount.release()) {
          delete p;
        }
      };
      return info;
    }();
    return &_;
  }

  static inline void doRegister() { SH_CORE::registerObjectType(vendorId, typeId, info); }

  // the following methods are generally used by the shard
  // that creates the object, that's it.
  // other shards use regular referenceVariable etc.

  T *New() {
    auto r = new ObjectRef();
    r->refcount.count = 1;
    return &r->shared;
  }

  std::tuple<T &, TOwnedVar<SH_CORE>> NewOwnedVar() {
    T *ptr = New();
    SHVar v = Get(ptr);
    return {*ptr, std::move((TOwnedVar<SH_CORE> &)v)};
  }

  Type GetObjectType() { return Type::Object(vendorId, typeId); }
  operator Type() { return GetObjectType(); }

  template <typename ST> T *Emplace(std::shared_ptr<ST> &&obj) {
    auto r = new ObjectRef{.shared = T(std::move(obj)), .refcount = RefCount<IsThreadSafe>{.count = 1}};
    return &r->shared;
  }

  void Release(T *obj) {
    auto r = reinterpret_cast<ObjectRef *>(obj);
    if (r->refcount.release()) {
      if constexpr (BeforeDelete != nullptr) {
        BeforeDelete(*obj);
      }
      delete r;
    }
  }

  uint32_t GetRefCount(T *obj) {
    auto r = reinterpret_cast<ObjectRef *>(obj);
    return r->refcount.count;
  }

  SHVar Get(T *obj) {
    SHVar res;
    res.valueType = SHType::Object;
    res.payload.objectValue = obj;
    res.payload.objectVendorId = vendorId;
    res.payload.objectTypeId = typeId;
    res.flags = SHVAR_FLAGS_USES_OBJINFO;
    res.objectInfo = &info;
    return res;
  }
};

template <typename T> struct ExtendedObjectTypeTraits {
  // using Type = T::Type;
};

template <class SH_CORE, typename T, typename Traits = ExtendedObjectTypeTraits<T>> class TExtendedObjectType {
  struct Extended {
    T data;
    RefCount1<true> refCount;
  };

public:
  static inline const SHExtendedObjectTypeInfo &getTypeInfo() {
    static SHExtendedObjectTypeInfo info = []() {
      SHExtendedObjectTypeInfo info{
          .reference =
              [](SHPointer ptr) {
                auto p = reinterpret_cast<Extended *>(ptr);
                p->refCount.reference();
              },
          .release =
              [](SHPointer ptr) {
                auto p = reinterpret_cast<Extended *>(ptr);
                if (p->refCount.release()) {
                  delete p;
                }
              },
      };
      if constexpr (detail::HasStaticMember_match<T, bool(const T &, const T &)>::value) {
        info.match = [](const void *self, const void *other) {
          auto tself = reinterpret_cast<T *>(self);
          auto tother = reinterpret_cast<T *>(other);
          return T::match(*tself, *tother);
        };
      }
      if constexpr (detail::HasStaticMember_hash<T, void(const T &, void *outDigest, size_t digestSize)>::value) {
        info.hash = [](const void *self, void *outDigest, size_t digestSize) {
          auto tself = reinterpret_cast<T *>(self);
          return T::hash(*tself, outDigest, digestSize);
        };
      }
      return info;
    }();
    return info;
  }

  // Create a new extended type into the target TypeInfo
  // returns the extened type info created, if one was already set,
  //  the new extended type is constructed from the old one using the copy constructor
  static inline Extended &makeExtended(TypeInfo &dst) {
    TypeInfo old;
    std::swap(old, dst);
    if (dst->basicType != SHType::Object) {
      dst = SHTypeInfo{};
    }

    Extended *e{};
    if (old->basicType == SHType::Object && old->object.extInfo) {
      e = new Extended(*reinterpret_cast<Extended *>(old->object.extInfoData));
    } else {
      e = new Extended();
    }

    const Type &type = Traits::Type;
    dst->object = type->object;
    dst->object.extInfo = &getTypeInfo();
    dst->object.extInfoData = e;

    return *e;
  }

  // static inline void doRegister() {
    // const Type &type = Traits::Type;
    // SH_CORE::registerObjectType(type->object.vendorId, type->object.typeId, info);
  // }

  // the following methods are generally used by the shard
  // that creates the object, that's it.
  // other shards use regular referenceVariable etc.

  // T *New() {
  //   auto r = new ObjectRef();
  //   r->refcount.count = 1;
  //   return &r->shared;
  // }

  // std::tuple<T &, TOwnedVar<SH_CORE>> NewOwnedVar() {
  //   T *ptr = New();
  //   SHVar v = Get(ptr);
  //   return {*ptr, std::move((TOwnedVar<SH_CORE> &)v)};
  // }

  // Type GetObjectType() { return Type::Object(vendorId, typeId); }
  // operator Type() { return GetObjectType(); }

  // template <typename ST> T *Emplace(std::shared_ptr<ST> &&obj) {
  //   auto r = new ObjectRef{.shared = T(std::move(obj)), .refcount = RefCount<IsThreadSafe>{.count = 1}};
  //   return &r->shared;
  // }

  // void Release(T *obj) {
  //   auto r = reinterpret_cast<ObjectRef *>(obj);
  //   if (r->refcount.release()) {
  //     if constexpr (BeforeDelete != nullptr) {
  //       BeforeDelete(*obj);
  //     }
  //     delete r;
  //   }
  // }

  // uint32_t GetRefCount(T *obj) {
  //   auto r = reinterpret_cast<ObjectRef *>(obj);
  //   return r->refcount.count;
  // }

  // SHVar Get(T *obj) {
  //   SHVar res;
  //   res.valueType = SHType::Object;
  //   res.payload.objectValue = obj;
  //   res.payload.objectVendorId = vendorId;
  //   res.payload.objectTypeId = typeId;
  //   res.flags = SHVAR_FLAGS_USES_OBJINFO;
  //   res.objectInfo = &info;
  //   return res;
  // }
};

}; // namespace shards

#endif /* C599F883_4CF5_416C_AC0D_AF07729F5C2B */
