#include "mesh_tree_drawable.hpp"
#include "transform_updater.hpp"
#include "../gizmos/shapes.hpp"
#include <stdexcept>
#include <boost/container/small_vector.hpp>

namespace gfx {
#if GFX_TRANSFORM_UPDATER_TRACK_VISITED
thread_local std::set<MeshTreeDrawable *> TransformUpdaterCollector::visited;
#endif

DrawablePtr MeshTreeDrawable::clone() const {
  CloningContext cc;
  return clone(cc);
}
DrawablePtr MeshTreeDrawable::clone(CloningContext &ctx) const {
  auto remapSkin = [&](const std::shared_ptr<Skin> &skin) -> std::shared_ptr<Skin> {
    auto it = ctx.skinMap.find(skin);
    if (it != ctx.skinMap.end())
      return it->second;
    auto newSkin = std::make_shared<Skin>(*skin.get());
    ctx.skinMap[skin] = newSkin;
    return newSkin;
  };

  MeshTreeDrawable::Ptr result = std::make_shared<MeshTreeDrawable>();
  result->name = name;
  result->trs = trs;
  for (auto &drawable : drawables) {
    auto newDrawable = std::static_pointer_cast<MeshDrawable>(drawable->clone());
    if (newDrawable->skin) {
      newDrawable->skin = remapSkin(newDrawable->skin);
    }
    result->drawables.push_back(newDrawable);
  }
  for (auto &child : children) {
    result->addChild(std::static_pointer_cast<MeshTreeDrawable>(child->clone(ctx)));
  }
  result->id = getNextDrawableId();
  return result;
}

DrawableProcessorConstructor MeshTreeDrawable::getProcessor() const { throw std::logic_error("not implemented"); }

static void updateSkin(const std::shared_ptr<const MeshTreeDrawable> &root, MeshDrawable &drawable,
                       const std::shared_ptr<Skin> &_skin) {
  auto &skin = *_skin.get();
  skin.jointMatrices.resize(skin.joints.size());
  skin.bounds = AABounds::empty();
  for (size_t i = 0; i < skin.joints.size(); ++i) {
    const auto &path = skin.joints[i];
    auto node = root->resolvePath(path);
    if (node) {
      skin.jointMatrices[i] =
          linalg::mul(linalg::mul(linalg::inverse(drawable.transform), node->resolvedTransform), skin.inverseBindMatrices[i]);
      skin.bounds.expand(extractTranslation(node->resolvedTransform));
    }
  }
}

bool MeshTreeDrawable::expand(shards::pmr::vector<const IDrawable *> &outDrawables,
                              shards::pmr::PolymorphicAllocator<> alloc) const {
  boost::container::small_vector<MeshDrawable *, 16> drawablesWithSkinsToUpdate;

  TransformUpdaterCollector collector(alloc);
  collector.collector = [&](const DrawablePtr &drawable, const float4x4& transform) {
    outDrawables.push_back(drawable.get());
    if (MeshDrawable *md = dynamic_cast<MeshDrawable *>(drawable.get())) {
      if (md->skin) {
        drawablesWithSkinsToUpdate.push_back(md);
      }
    }
  };
  collector.update(const_cast<MeshTreeDrawable &>(*this));

  auto rootNode = shared_from_this();
  for (auto &drawable : drawablesWithSkinsToUpdate) {
    updateSkin(rootNode, *drawable, drawable->skin);
  }
  return true;
}

void MeshTreeDrawable::debugVisualize(ShapeRenderer &sr) {
  this->foreach (this->shared_from_this(), [&](Ptr node) {
    if (auto parent = node->parent.lock()) {
      sr.addLine(extractTranslation(node->resolvedTransform), extractTranslation(parent->resolvedTransform), float4(0, 1, 0, 1),
                 1);
    }
  });
}

} // namespace gfx
