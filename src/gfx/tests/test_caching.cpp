#include <gfx/renderer_cache.hpp>
#include <gfx/drawables/mesh_drawable.hpp>
#include <gfx/feature.hpp>
#include <catch2/catch_all.hpp>

using namespace gfx;
using std::make_shared;

TEST_CASE("Unique Ids", "[Caching]") {
  SECTION("Uniqueness") {
    // Reset counters
    UniqueIdGenerator::_resetAll();

    MeshPtr meshes[] = {
        make_shared<Mesh>(),
        make_shared<Mesh>(),
    };

    MeshDrawable::Ptr drawables[] = {
        make_shared<MeshDrawable>(meshes[0]),
        make_shared<MeshDrawable>(meshes[1]),
    };

    // Check that counters have actually been reset
    CHECK(drawables[0]->getId() == withTag(0, UniqueIdTag::Drawable));
    CHECK(meshes[0]->getId() == withTag(0, UniqueIdTag::Mesh));

    CHECK(meshes[0]->getId() != meshes[1]->getId());
    CHECK(drawables[0]->getId() != drawables[1]->getId());

    auto feature = make_shared<Feature>();
    auto material = make_shared<Material>();
    auto texture = make_shared<Texture>();
    auto step0 = make_shared<RenderDrawablesStep>();
    auto step1 = make_shared<RenderFullscreenStep>();
    auto step2 = make_shared<ClearStep>();

    std::vector<UniqueId> ids = {
        drawables[0]->getId(), drawables[1]->getId(), meshes[0]->getId(), meshes[1]->getId(), feature->getId(),
        material->getId(),     texture->getId(),      step0->getId(),     step1->getId(),     step2->getId(),
    };

    // Check all are unique
    std::set<UniqueId> encountered;
    for (auto id : ids) {
      CHECK(!encountered.contains(id));
      encountered.insert(id);
    }
  }

  SECTION("Id Reuse") {
    // Reset counters
    UniqueIdGenerator::_resetAll();

    std::set<UniqueId> prevIds;
    {
      auto mesh0 = make_shared<Mesh>();
      auto mesh1 = make_shared<Mesh>();

      CHECK(mesh0->getId() == withTag(0, UniqueIdTag::Mesh));
      CHECK(mesh1->getId() == withTag(1, UniqueIdTag::Mesh));
      prevIds.insert(mesh0->getId());
      prevIds.insert(mesh1->getId());

      // Meshes are now freed
    }

    // These should get new Ids
    auto mesh0 = make_shared<Mesh>();
    auto mesh1 = make_shared<Mesh>();
    CHECK(!prevIds.contains(mesh0->getId()));
    CHECK(!prevIds.contains(mesh1->getId()));
  }

  SECTION("Cloning") {
    // Reset counters
    UniqueIdGenerator::_resetAll();

    auto mesh0 = make_shared<Mesh>();
    CHECK(mesh0->getId() != clone(mesh0)->getId());

    auto texture0 = make_shared<Texture>();
    CHECK(texture0->getId() != clone(texture0)->getId());

    auto drawable0 = make_shared<MeshDrawable>(mesh0);
    CHECK(drawable0->getId() != clone(drawable0)->getId());

    auto material0 = make_shared<Material>();
    CHECK(material0->getId() != clone(material0)->getId());
  }
}

TEST_CASE("Drawable caching", "[Caching]") {
  using namespace gfx::detail;

  FeaturePtr features[] = {
      make_shared<Feature>(),
      make_shared<Feature>(),
      make_shared<Feature>(),
      make_shared<Feature>(),
  };

  MeshPtr meshes[] = {
      make_shared<Mesh>(),
      make_shared<Mesh>(),
  };

  MeshDrawable::Ptr drawables[4] = {
      make_shared<MeshDrawable>(meshes[0]),
      make_shared<MeshDrawable>(meshes[0]),
      make_shared<MeshDrawable>(meshes[0]),
      make_shared<MeshDrawable>(meshes[1]),
  };

  TexturePtr textures[4] = {
      make_shared<Texture>(),
      make_shared<Texture>(),
      make_shared<Texture>(),
      make_shared<Texture>(),
  };

  MaterialPtr materials[4] = {
      make_shared<Material>(),
      make_shared<Material>(),
      make_shared<Material>(),
      make_shared<Material>(),
  };

  materials[2]->parameters.set("mainColor", textures[1]);

  drawables[0]->features.push_back(features[0]);
  drawables[0]->features.push_back(features[2]);
  drawables[0]->material = materials[2];
  drawables[0]->parameters.set("normalMap", textures[3]);

  drawables[1] = clone(drawables[0]);

  HashCollector collector0;
  drawables[0]->staticHashCollect(collector0);

  HashCollector collector1;
  drawables[1]->staticHashCollect(collector1);

  Hash128 hash0 = collector0.hasher.getDigest();
  Hash128 hash1 = collector1.hasher.getDigest();
  CHECK(hash0 == hash1);
}
