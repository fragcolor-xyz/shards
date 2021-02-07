#include <random>

#include "../../include/ops.hpp"
#include "../../include/utility.hpp"
#include "../core/runtime.hpp"

#undef CHECK

#define CATCH_CONFIG_RUNNER

#include <catch2/catch_all.hpp>

int main(int argc, char *argv[]) {
  chainblocks::Globals::RootPath = "./";
  registerCoreBlocks();
  int result = Catch::Session().run(argc, argv);
#ifdef __EMSCRIPTEN_PTHREADS__
  // in this case we need to call exit our self
  exit(0);
#endif
  return result;
}

namespace chainblocks {
namespace BGFX_Tests {
  extern void testVertexAttribute();
  extern void testModel();
  extern void testCamera();
  extern void testDraw();
  extern void testUniforms();
  extern void testShaderCompiler();
}
namespace GLTF_Tests {
  extern void testLoad();
}
}

using namespace chainblocks;

TEST_CASE("GFX-VertexAttribute", "[gfx]") {
  BGFX_Tests::testVertexAttribute();
}

TEST_CASE("GFX-Model", "[gfx]") {
  BGFX_Tests::testModel();
}

TEST_CASE("GFX-Camera", "[gfx]") {
  BGFX_Tests::testCamera();
}

TEST_CASE("GFX-Draw", "[gfx]") {
  BGFX_Tests::testDraw();
}

TEST_CASE("GFX-Uniforms", "[gfx]") {
  BGFX_Tests::testUniforms();
}

TEST_CASE("GFX-ShaderCompiler", "[gfx]") { //[!mayfail]
  BGFX_Tests::testShaderCompiler();
}

TEST_CASE("GLTF-Load", "[gltf]") {
  GLTF_Tests::testLoad();
}
