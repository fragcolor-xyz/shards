#include "texture.hpp"
#include "drawable.hpp"
#include "buffer.hpp"
#include "context_data.hpp"
#include "context.hpp"
#include <boost/container/flat_map.hpp>
#include <boost/container/stable_vector.hpp>

namespace gfx::detail {

template <typename T>
  requires gfx::TWithContextData<T>
struct SubContextDataStorage {
  using ContextDataType = typename T::ContextDataType;
  boost::container::flat_map<size_t, ContextDataType, std::less<size_t>,
                             boost::container::stable_vector<std::pair<size_t, ContextDataType>>>
      map;

  ContextDataType &getCreateOrUpdate(Context &context, size_t frameCounter, const std::shared_ptr<T> &v) {
    auto id = v->getId().getIdPart();
    auto it = map.find(id);
    if (it == map.end()) {
      it = map.emplace(id, ContextDataType()).first;
      v->initContextData(context, it->second);
    }

    auto &cd = it->second;

    if (cd.lastChecked != frameCounter) {
      if (v->getVersion() != cd.version) {
        v->updateContextData(context, cd);
        cd.version = v->getVersion();
        cd.lastChecked = frameCounter;
      }
    }
    return cd;
  }
};

struct ContextDataStorage {
  SubContextDataStorage<gfx::Texture> textures;
  SubContextDataStorage<gfx::Mesh> meshes;
  SubContextDataStorage<gfx::Buffer> buffers;

  template <typename T> auto &getCreateOrUpdate(Context &context, size_t frameCounter, const std::shared_ptr<T> &v) {
    if constexpr (std::is_same_v<T, gfx::Texture>) {
      return textures.getCreateOrUpdate(context, frameCounter, v);
    } else if constexpr (std::is_same_v<T, gfx::Mesh>) {
      return meshes.getCreateOrUpdate(context, frameCounter, v);
    } else if constexpr (std::is_same_v<T, gfx::Buffer>) {
      return buffers.getCreateOrUpdate(context, frameCounter, v);
    } else {
      throw std::logic_error("Invalid type");
    }
  }
};

struct ContextDataCollector {
  template <typename T>
    requires gfx::TWithContextData<T>
  struct Fixup {
    using Type = typename T::ContextDataType;
    // The context data pointer to fix up
    Type **pointer;
    std::shared_ptr<T> original;
    Fixup(Type **ptr, const std::shared_ptr<T> &original) : pointer(ptr), original(original) {}
  };
  std::vector<Fixup<Texture>> fixupTextures;
  std::vector<Fixup<Mesh>> fixupMeshes;
  std::vector<Fixup<Buffer>> fixupBuffers;

  void reset() {
    fixupTextures.clear();
    fixupMeshes.clear();
    fixupBuffers.clear();
  }

  template <typename T> void fixup(const typename Fixup<T>::Type *&contextDataPtr, const std::shared_ptr<T> &v) {
    fixup(const_cast<typename Fixup<T>::Type *&>(contextDataPtr), v);
  }

  template <typename T> void fixup(typename Fixup<T>::Type *&contextDataPtr, const std::shared_ptr<T> &v) {
    fixup(&contextDataPtr, v);
  }
  template <typename T> void fixup(typename Fixup<T>::Type **contextDataPtr, const std::shared_ptr<T> &v) {
    if constexpr (std::is_same_v<T, gfx::Texture>) {
      fixupTextures.emplace_back(contextDataPtr, v);
    } else if constexpr (std::is_same_v<T, gfx::Mesh>) {
      fixupMeshes.emplace_back(contextDataPtr, v);
    } else if constexpr (std::is_same_v<T, gfx::Buffer>) {
      fixupBuffers.emplace_back(contextDataPtr, v);
    } else {
      throw std::logic_error("Invalid type");
    }
  }

  template <typename T>
  void doFixup(std::vector<Fixup<T>> &vec, ContextDataStorage &storage, Context &context, size_t frameCounter) {
    for (auto &v : vec) {
      auto &cd = storage.getCreateOrUpdate(context, frameCounter, v.original);
      if (v.pointer) {
        *v.pointer = &cd;
      }
    }
    vec.clear();
  }
  void doFixup(ContextDataStorage &storage, Context &context, size_t frameCounter) {
    doFixup(fixupTextures, storage, context, frameCounter);
    doFixup(fixupMeshes, storage, context, frameCounter);
    doFixup(fixupBuffers, storage, context, frameCounter);
  }
};

} // namespace gfx::detail
