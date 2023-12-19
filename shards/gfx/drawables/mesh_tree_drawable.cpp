#include "mesh_tree_drawable.hpp"
#include "transform_updater.hpp"
#include <stdexcept>
#include <boost/container/small_vector.hpp>

namespace gfx {

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
  for (size_t i = 0; i < skin.joints.size(); ++i) {
    const auto &path = skin.joints[i];
    auto node = root->resolvePath(path);
    if (node) {
      skin.jointMatrices[i] =
          linalg::mul(linalg::mul(linalg::inverse(drawable.transform), node->resolvedTransform), skin.inverseBindMatrices[i]);
    }
  }
}

bool MeshTreeDrawable::expand(shards::pmr::vector<const IDrawable *> &outDrawables) const {
  boost::container::small_vector<MeshDrawable *, 16> skinsToUpdate;

  TransformUpdaterCollector collector;
  collector.collector = [&](const DrawablePtr &drawable) {
    outDrawables.push_back(drawable.get());
    if (MeshDrawable *md = dynamic_cast<MeshDrawable *>(drawable.get())) {
      if (md->skin) {
        skinsToUpdate.push_back(md);
      }
    }
  };
  collector.update(const_cast<MeshTreeDrawable &>(*this));

  auto rootNode = shared_from_this();
  for (auto &skin : skinsToUpdate) {
    updateSkin(rootNode, *skin, skin->skin);
  }
  return true;
}

} // namespace gfx
