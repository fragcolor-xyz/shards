#ifndef C599F883_4CF5_416C_AC0D_AF07729F5C2B
#define C599F883_4CF5_416C_AC0D_AF07729F5C2B

#include <boost/tti/has_static_member_function.hpp>
#include <boost/tti/has_member_function.hpp>

namespace shards {

namespace detail {
BOOST_TTI_TRAIT_HAS_STATIC_MEMBER_FUNCTION(HasStaticMember_serialize, serialize);
BOOST_TTI_TRAIT_HAS_STATIC_MEMBER_FUNCTION(HasStaticMember_deserialize, deserialize);
BOOST_TTI_TRAIT_HAS_STATIC_MEMBER_FUNCTION(HasStaticMember_beforeDelete, beforeDelete);
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
} // namespace detail

template <typename T> struct ObjectInfo {
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

template <class SH_CORE, typename T, typename TObjectInfo = ObjectInfo<T>> class TObjectType {
private:
  static constexpr bool IsThreadSafe = detail::ObjectInfo_IsThreadSafe<TObjectInfo>::value;
  static inline const char *Name = detail::ObjectInfo_Name<T>::value;

  SHObjectInfo info;
  int32_t vendorId;
  int32_t typeId;

  struct ObjectRef {
    T shared;
    RefCount<detail::ObjectInfo_IsThreadSafe<TObjectInfo>::value> refcount;
  };

public:
  static inline const SHObjectInfo *getInfo() {
    static SHObjectInfo _ = []() {
      SHObjectInfo info{};
      info.name = Name;
      info.isThreadSafe = IsThreadSafe;
      if constexpr (detail::HasMember_serialize<T>::value) {
        info.serialize = [](SHPointer obj, uint8_t **outData, uint64_t *outLen, SHPointer *customHandle) {
          auto tobj = reinterpret_cast<T *>(obj);
          auto holder = new std::vector<uint8_t>();
          
          TObjectInfo::serialize(*tobj, *holder);
          *customHandle = holder;
          *outData = holder->data();
          *outLen = holder->size();
          return true;
        };
      }
      if constexpr (detail::HasMember_deserialize<T>::value) {
        info.serialize = [](SHPointer obj, uint8_t **outData, uint64_t *outLen, SHPointer *customHandle) {
          auto tobj = reinterpret_cast<T *>(obj);
          auto holder = new std::vector<uint8_t>();
          TObjectInfo::serialize(*tobj, *holder);
          *customHandle = holder;
          *outData = holder->data();
          *outLen = holder->size();
          return true;
        };
      }
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

  TObjectType(const char *name, int32_t vendorId, int32_t typeId) : vendorId(vendorId), typeId(typeId) {
    info = {};
    // if (Serializer != nullptr && Deserializer != nullptr) {
    //   info.serialize = [](SHPointer obj, uint8_t **outData, uint64_t *outLen, SHPointer *customHandle) {
    //     auto tobj = reinterpret_cast<T *>(obj);
    //     auto holder = new std::vector<uint8_t>();
    //     *holder = Serializer(*tobj);
    //     *customHandle = holder;
    //     *outData = holder->data();
    //     *outLen = holder->size();
    //     return true;
    //   };
    //   info.free = [](SHPointer handle) {
    //     auto holder = reinterpret_cast<std::vector<uint8_t> *>(handle);
    //     delete holder;
    //   };
    //   info.deserialize = [](uint8_t *data, uint64_t len) {
    //     auto r = new ObjectRef();
    //     r->shared = Deserializer(std::string_view((char *)data, len));
    //     // don't bump ref count, deserializer is supposed to do that
    //     return (SHPointer)r;
    //   };
    // }
    SH_CORE::registerObjectType(vendorId, typeId, info);
  }

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
}; // namespace shards

#endif /* C599F883_4CF5_416C_AC0D_AF07729F5C2B */
