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
    CHECK(drawables[0]->getId() == UniqueId(0).withTag(UniqueIdTag::Drawable));
    CHECK(meshes[0]->getId() == UniqueId(0).withTag(UniqueIdTag::Mesh));

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

      CHECK(mesh0->getId() == UniqueId(0).withTag(UniqueIdTag::Mesh));
      CHECK(mesh1->getId() == UniqueId(1).withTag(UniqueIdTag::Mesh));
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
  };

  MeshPtr meshes[] = {
      make_shared<Mesh>(),
      make_shared<Mesh>(),
  };

  MeshDrawable::Ptr drawables[] = {
      make_shared<MeshDrawable>(meshes[0]),
      make_shared<MeshDrawable>(meshes[0]),
  };

  TexturePtr textures[] = {
      make_shared<Texture>(),
      make_shared<Texture>(),
  };

  MaterialPtr materials[] = {
      make_shared<Material>(),
  };

  materials[0]->parameters.set("mainColor", textures[0]);

  drawables[0]->features.push_back(features[0]);
  drawables[0]->features.push_back(features[1]);
  drawables[0]->material = materials[0];
  drawables[0]->parameters.set("normalMap", textures[1]);

  drawables[1] = clone(drawables[0]);

  PipelineHashCollector collector0;
  drawables[0]->pipelineHashCollect(collector0);

  PipelineHashCollector collector1;
  drawables[1]->pipelineHashCollect(collector1);

  Hash128 hash0 = collector0.hasher.getDigest();
  Hash128 hash1 = collector1.hasher.getDigest();
  CHECK(hash0 == hash1);

  SECTION("Dynamic change doesn't change hashes") {
    collector1.reset();
    drawables[1]->clipRect = int4(100, 200, 300, 400);
    drawables[1]->pipelineHashCollect(collector1);
    hash1 = collector1.hasher.getDigest();
    CHECK(hash0 == hash1);

    collector1.reset();
    drawables[1]->parameters.set("SomeVariable", float4(1.0f));
    drawables[1]->pipelineHashCollect(collector1);
    hash1 = collector1.hasher.getDigest();
    CHECK(hash0 == hash1);
  }

  SECTION("Changing references to similar objects doesn't change hashes") {
    collector1.reset();
    drawables[1]->mesh = meshes[1];
    drawables[1]->pipelineHashCollect(collector1);
    hash1 = collector1.hasher.getDigest();
    CHECK(hash0 == hash1);
  }

  SECTION("Add reference to changed object changes hashes") {
    collector1.reset();
    drawables[1]->features.push_back(features[1]);
    drawables[1]->pipelineHashCollect(collector1);
    hash1 = collector1.hasher.getDigest();
    CHECK(hash0 != hash1);
  }
}

TEST_CASE("Pipeline cache evictions", "[Caching]") {
  MaterialPtr materials[] = {
      make_shared<Material>(),
  };

  FeaturePtr features[] = {
      make_shared<Feature>(),
      make_shared<Feature>(),
  };

  MeshPtr meshes[] = {
      make_shared<Mesh>(),
      make_shared<Mesh>(),
  };

  meshes[0]->update(
      MeshFormat{
          .indexFormat = IndexFormat::UInt16,
          .vertexAttributes = {MeshVertexAttribute("a", 1, StorageType::UInt8)},
      },
      std::vector<uint8_t>{0});
  meshes[1]->update(
      MeshFormat{
          .indexFormat = IndexFormat::UInt32,
          .vertexAttributes = {MeshVertexAttribute("b", 1, StorageType::UInt8)},
      },
      std::vector<uint8_t>{0});

  MeshDrawable::Ptr drawables[] = {
      make_shared<MeshDrawable>(meshes[0]),
      make_shared<MeshDrawable>(meshes[0]),
  };

  detail::PipelineHashCache cache;

  Hash128 hash0 = cache.update(*drawables[0].get());
  CHECK(cache.find(drawables[0]->getId()) != nullptr);
  CHECK(*cache.find(drawables[0]->getId()) == hash0);

  Hash128 hash1 = cache.update(*drawables[1].get());
  CHECK(cache.find(drawables[1]->getId()) != nullptr);
  CHECK(*cache.find(drawables[1]->getId()) == hash1);

  // Same object
  CHECK(hash0 == hash1);

  drawables[1]->mesh = meshes[1];

  // Check updated hash
  hash1 = cache.update(drawables[1]);
  CHECK(cache.find(drawables[1]->getId()) != nullptr);
  CHECK(*cache.find(drawables[1]->getId()) == hash1);

  CHECK(hash0 != hash1);
}
