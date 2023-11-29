#include "boost/container/pmr/global_resource.hpp"
#include <gfx/renderer_cache.hpp>
#include <gfx/drawables/mesh_drawable.hpp>
#include <gfx/renderer_storage.hpp>
#include <gfx/drawable_processor.hpp>
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
    auto step2 = make_shared<NoopStep>();

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

  Context context;
  context.init();
  RendererStorage storage(context);

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

  auto processorConstructor = drawables[0]->getProcessor();
  auto processor = processorConstructor(context);

  auto computeHashes = [&](Hash128(&outHashes)[2]) {
    shards::pmr::vector<Feature *> sharedFeatures = {};
    shards::pmr::vector<const IDrawable *> idrawables = {
        drawables[0].get(),
        drawables[1].get(),
    };

    BuildPipelineOptions opts{};
    DrawablePreprocessContext ctx{
        .drawables = idrawables,
        .features = sharedFeatures,
        .buildPipelineOptions = opts,
        .storage = storage,
        .sharedHash = Hash128{},
        .outHashes = outHashes,
    };
    processor->preprocess(ctx);
  };

  Hash128 hashes[2];
  computeHashes(hashes);
  CHECK(hashes[0] == hashes[1]);

  SECTION("Dynamic change doesn't change hashes") {
    drawables[1]->clipRect = int4(100, 200, 300, 400);
    Hash128 hashes1[2];
    computeHashes(hashes1);
    CHECK(hashes[1] == hashes1[1]);

    drawables[1]->parameters.set("SomeVariable", float4(1.0f));
    Hash128 hashes2[2];
    computeHashes(hashes2);
    CHECK(hashes[1] == hashes2[1]);
  }

  SECTION("Changing references to similar objects doesn't change hashes") {
    drawables[1]->mesh = meshes[1];

    Hash128 hashes1[2];
    computeHashes(hashes1);
    CHECK(hashes[1] == hashes1[1]);
  }

  SECTION("Add reference to changed object changes hashes") {
    drawables[1]->features.push_back(features[1]);

    Hash128 hashes1[2];
    computeHashes(hashes1);
    CHECK(hashes[1] != hashes1[1]);
  }
}
