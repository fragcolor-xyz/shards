#include "./context.hpp"
#include "gfx/enums.hpp"
#include "gfx/shader/log.hpp"
#include "gfx/shader/types.hpp"
#include <catch2/catch_all.hpp>
#include <cctype>
#include <gfx/context.hpp>
#include <gfx/shader/blocks.hpp>
#include <gfx/shader/generator.hpp>
#include <gfx/shader/buffer_serializer.hpp>
#include <gfx/buffer.hpp>
#include <gfx/gfx_wgpu_pipeline_helper.hpp>
#include <spdlog/spdlog.h>

using namespace gfx;
using namespace gfx::shader;
using String = std::string;

bool compareNoWhitespace(const String &a, const String &b) {
  using Iterator = String::const_iterator;
  using StringView = std::string_view;

  auto isWhitespace = [](const Iterator &it) { return *it == '\n' || *it == '\t' || *it == '\r' || *it == ' '; };
  auto isSymbol = [](const Iterator &it) { return !std::isalnum(*it); };

  // Skip whitespace, returns true on end of string
  auto skipWhitespace = [=](Iterator &it, const Iterator &end) {
    do {
      if (isWhitespace(it)) {
        it++;
      } else {
        return false;
      }
    } while (it != end);
    return true;
  };

  auto split = [=](StringView &sv, Iterator &it, const Iterator &end) {
    if (skipWhitespace(it, end))
      return false;

    auto itStart = it;
    size_t len = 0;
    bool hasSymbols = false;
    do {
      if (isSymbol(it)) {
        hasSymbols = true;
        if (len > 0)
          break;
      }
      it++;
      len++;
    } while (it != end && !hasSymbols && !isWhitespace(it));
    sv = StringView(&*itStart, len);
    return true;
  };

  auto itA = a.cbegin();
  auto itB = b.cbegin();
  auto shouldAbort = [&]() { return itA == a.end() || itB == b.end(); };

  while (!shouldAbort()) {
    std::string_view svA;
    std::string_view svB;
    if (!split(svA, itA, a.end()))
      break;
    if (!split(svB, itB, b.end()))
      break;
    if (svA != svB)
      return false;
  }

  bool endA = itA == a.end() ? true : skipWhitespace(itA, a.end());
  bool endB = itB == b.end() ? true : skipWhitespace(itB, b.end());
  return endA == endB;
}

TEST_CASE("Compare & ignore whitespace", "[Shader]") {
  String a = R"(string with whitespace)";
  String b = R"(
string 	with
	whitespace

)";
  CHECK(compareNoWhitespace(a, b));

  String c = "stringwithwhitespace";
  CHECK(!compareNoWhitespace(a, c));

  String d = "string\rwith\nwhitespace";
  CHECK(compareNoWhitespace(a, d));

  String e = "string\twith whitespace";
  CHECK(compareNoWhitespace(a, e));

  String f = "string\t";
  CHECK(!compareNoWhitespace(a, f));
}

static bool validateShaderModule(Context &context, const String &code) {
  try {
    compileShaderFromWgsl(context.wgpuDevice, code.c_str());
  } catch (...) {
    return false;
  }
  return true;
}

void setupDefaultBindings(shader::Generator &generator) {
  {
    StructType structType;
    structType->addField("world", Types::Float4x4);
    auto &binding = generator.bufferBindings.emplace_back();
    binding.bindGroup = 0;
    binding.binding = 0;
    binding.name = "object";
    binding.layout = structType;
    binding.addressSpace = AddressSpace::Storage;
  }

  {
    StructType structType;
    structType->addField("viewProj", Types::Float4x4);
    auto &binding = generator.bufferBindings.emplace_back();
    binding.bindGroup = 1;
    binding.binding = 0;
    binding.name = "view";
    binding.layout = structType;
    binding.addressSpace = AddressSpace::Uniform;
  }
}

TEST_CASE("Shader basic", "[Shader]") {
  Generator generator;
  generator.meshFormat.vertexAttributes = {
      MeshVertexAttribute("position", 3),
      MeshVertexAttribute("normal", 3),
      MeshVertexAttribute("texcoord0", 2),
      MeshVertexAttribute("texcoord1", 3),
  };

  std::shared_ptr<TestContext> context = createTestContext();

  setupDefaultBindings(generator);

  auto colorFieldType = NumType(ShaderFieldBaseType::Float32, 4);
  auto positionFieldType = NumType(ShaderFieldBaseType::Float32, 4);

  generator.outputFields.emplace_back("color", colorFieldType);

  std::vector<EntryPoint> entryPoints;
  auto vec4Pos = blocks::makeCompoundBlock("vec4<f32>(", blocks::ReadInput("position"), ".xyz, 1.0)");
  entryPoints.emplace_back(
      "position", ProgrammableGraphicsStage::Vertex,
      blocks::makeCompoundBlock(blocks::WriteOutput("position", positionFieldType, std::move(vec4Pos), "*",
                                                    blocks::ReadBuffer("world", Types::Float4x4), "*",
                                                    blocks::ReadBuffer("viewProj", Types::Float4x4, "view"))));

  entryPoints.emplace_back(
      "color", ProgrammableGraphicsStage::Fragment,
      blocks::makeCompoundBlock(blocks::WriteOutput("color", colorFieldType, "vec4<f32>(0.0, 1.0, 0.0, 1.0);")));

  GeneratorOutput output = generator.build(entryPoints);
  SPDLOG_INFO(output.wgslSource);

  GeneratorOutput::dumpErrors(shader::getLogger(), output);
  CHECK(output.errors.empty());

  CHECK(validateShaderModule(*context.get(), output.wgslSource));
}

TEST_CASE("Shader globals & dependencies", "[Shader]") {
  Generator generator;
  generator.meshFormat.vertexAttributes = {
      MeshVertexAttribute("position", 3),
  };

  std::shared_ptr<TestContext> context = createTestContext();

  setupDefaultBindings(generator);

  auto colorFieldType = NumType(ShaderFieldBaseType::Float32, 4);
  auto positionFieldType = NumType(ShaderFieldBaseType::Float32, 4);

  generator.outputFields.emplace_back("color", colorFieldType);

  std::vector<EntryPoint> entryPoints;
  auto vec4Pos = blocks::makeCompoundBlock("vec4<f32>(", blocks::ReadInput("position"), ".xyz, 1.0)");
  entryPoints.emplace_back(
      "position", ProgrammableGraphicsStage::Vertex,
      blocks::makeCompoundBlock(blocks::WriteOutput("position", positionFieldType, std::move(vec4Pos), "*",
                                                    blocks::ReadBuffer("world", Types::Float4x4), "*",
                                                    blocks::ReadBuffer("viewProj", Types::Float4x4, "view"))));

  entryPoints.emplace_back(
      "colorDefault", ProgrammableGraphicsStage::Fragment,
      blocks::makeCompoundBlock(blocks::WriteGlobal("color", colorFieldType, "vec4<f32>(0.0, 1.0, 0.0, 1.0);")));

  entryPoints.emplace_back("color", ProgrammableGraphicsStage::Fragment,
                           blocks::makeCompoundBlock(blocks::WriteOutput("color", colorFieldType, blocks::ReadGlobal("color"))));
  entryPoints.back().dependencies.emplace_back("colorDefault", DependencyType::After);

  GeneratorOutput output = generator.build(entryPoints);
  SPDLOG_INFO(output.wgslSource);

  GeneratorOutput::dumpErrors(shader::getLogger(), output);
  CHECK(output.errors.empty());

  CHECK(validateShaderModule(*context.get(), output.wgslSource));
}

void interpolateTexcoord(GeneratorContext &context) {}

TEST_CASE("Shader textures", "[Shader]") {
  Generator generator;
  generator.meshFormat.vertexAttributes = {
      MeshVertexAttribute("position", 3),
      MeshVertexAttribute("texCoord0", 2),
  };

  std::shared_ptr<TestContext> context = createTestContext();

  auto colorFieldType = NumType(ShaderFieldBaseType::Float32, 4);
  auto positionFieldType = NumType(ShaderFieldBaseType::Float32, 4);

  TextureBindingLayoutBuilder textureLayoutBuilder;
  textureLayoutBuilder.addOrUpdateSlot("baseColorTexture", TextureDimension::D2, 0);

  generator.textureBindingLayout = textureLayoutBuilder.finalize(0);
  generator.outputFields.emplace_back("position", positionFieldType);

  std::vector<EntryPoint> entryPoints;
  entryPoints.emplace_back("color", ProgrammableGraphicsStage::Fragment,
                           blocks::WriteOutput("color", colorFieldType, blocks::SampleTexture("baseColorTexture")));
  entryPoints.emplace_back("interpolate", ProgrammableGraphicsStage::Vertex, blocks::DefaultInterpolation());

  // Need dummy position
  entryPoints.emplace_back("position", ProgrammableGraphicsStage::Vertex,
                           blocks::makeCompoundBlock(blocks::WriteOutput("position", positionFieldType, "vec4<f32>(0.0)")));

  GeneratorOutput output = generator.build(entryPoints);
  SPDLOG_INFO(output.wgslSource);

  GeneratorOutput::dumpErrors(shader::getLogger(), output);
  CHECK(output.errors.empty());

  CHECK(validateShaderModule(*context.get(), output.wgslSource));
}

TEST_CASE("Shader storage", "[Shader]") {
  Generator generator;

  std::shared_ptr<TestContext> context = createTestContext();

  auto positionFieldType = NumType(ShaderFieldBaseType::Float32, 4);

  generator.outputFields.emplace_back("position", positionFieldType);
  {
    auto &buf = generator.bufferBindings.emplace_back();
    buf.name = "data";
    buf.bindGroup = 1;
    buf.dimension = dim::One{};
    buf.addressSpace = AddressSpace::Storage;

    StructType t;
    t->entries.emplace_back("const_f", NumType(ShaderFieldBaseType::Float32, 1));
    t->entries.emplace_back("counter", NumType(ShaderFieldBaseType::Int32, 1).asAtomic());
    StructType t1;
    t1->addField("elements", ArrayType(t));
    buf.layout = t1;
  }
  {
    auto &buf = generator.bufferBindings.emplace_back();
    buf.name = "data1";
    buf.bindGroup = 1;
    buf.binding = 1;
    buf.dimension = dim::PerInstance{};
    buf.addressSpace = AddressSpace::Storage;
    buf.layout = Types::Int4;
  }
  {
    auto &buf = generator.bufferBindings.emplace_back();
    buf.name = "fixeddata2";
    buf.bindGroup = 1;
    buf.binding = 1;
    buf.dimension = dim::Fixed(200);
    buf.addressSpace = AddressSpace::Uniform;
    buf.layout = Types::Int4;
  }

  std::vector<EntryPoint> entryPoints;

  // Need dummy position
  entryPoints.emplace_back("position", ProgrammableGraphicsStage::Vertex,
                           blocks::makeCompoundBlock(blocks::WriteOutput("position", positionFieldType, "vec4<f32>(0.0)")));

  GeneratorOutput output = generator.build(entryPoints);
  SPDLOG_INFO(output.wgslSource);

  GeneratorOutput::dumpErrors(shader::getLogger(), output);
  CHECK(output.errors.empty());

  CHECK(validateShaderModule(*context.get(), output.wgslSource));
}

TEST_CASE("Layout generation", "[Shader]") {
  StructType innerSt;
  innerSt->addField("a", Types::Float);
  StructType st;
  st->addField("inner0", innerSt);
  st->addField("inner1", innerSt);

  StructLayoutBuilder lb(shader::AddressSpace::StorageRW);
  lb.pushFromStruct(st);
  auto layout = lb.finalize();

  CHECK(layout.fieldNames.size() == 2);
  CHECK(layout.size == 8);
  CHECK(layout.maxAlignment == 4);
}

TEST_CASE("Layout generation, runtime sized", "[Shader]") {
  StructType innerSt;
  innerSt->addField("a", Types::Float);
  StructType st;
  st->addField("inner0", innerSt);
  st->addField("inner1", ArrayType(innerSt));

  StructLayoutBuilder lb(shader::AddressSpace::StorageRW);
  lb.pushFromStruct(st);

  auto layout = lb.finalize();
  auto structMap = lb.getStructMap();

  CHECK(layout.isRuntimeSized);
  CHECK(layout.fieldNames.size() == 2);
  CHECK(layout.size == 8);
  CHECK(layout.maxAlignment == 4);

  LayoutTraverser lt(layout, structMap);
  auto ref = lt.findRuntimeSizedArray();
  CHECK(ref.offset == 4);
  CHECK(ref.path != LayoutPath({"something", "else"})); // sanity check
  CHECK(ref.path == LayoutPath({"inner1"}));

  auto refA = lt.find({"inner0", "a"});
  CHECK(refA);
  if (refA) {
    CHECK(refA->offset == 0);
    CHECK(refA->size == 4);
    CHECK(refA->type != Type(innerSt)); // sanity check
    CHECK(refA->type == Types::Float);
  }
}

TEST_CASE("Buffer IO", "[Shader]") {
  StructType innerSt0;
  innerSt0->addField("four", Types::UInt32);
  innerSt0->addField("normal", Types::Float3);
  innerSt0->addField("pi", Types::Float);
  StructType innerSt1;
  innerSt1->addField("normal0", Types::Float);
  innerSt1->addField("normal1", Types::Int32);
  StructType st;
  st->addField("inner0", innerSt0);
  st->addField("inner1", ArrayType(innerSt1));

  StructLayoutBuilder lb(shader::AddressSpace::StorageRW);
  lb.pushFromStruct(st);

  auto layout = lb.finalize();
  auto structMap = lb.getStructMap();

  CHECK(layout.isRuntimeSized);
  CHECK(layout.size == 48);
  CHECK(layout.maxAlignment == 16);

  LayoutTraverser lt(layout, structMap);
  auto rtRef = lt.findRuntimeSizedArray();
  CHECK(rtRef.offset == 32);

  size_t elementCount = 6;
  size_t allocBufferSize = runtimeBufferSizeHelper(layout, rtRef, elementCount);
  CHECK(allocBufferSize == alignTo(32 + 8 * elementCount, layout.maxAlignment));

  std::shared_ptr<TestContext> context = createTestContext();

  std::vector<uint8_t> data;
  data.resize(allocBufferSize);
  BufferSerializer a(data);

  float3 testVec3 = linalg::normalize(float3(0.1f, 1.0f, 0.1f));

  // Write data
  {
    auto f = lt.find({"inner0", "four"});
    CHECK(f);
    a.write(*f, 4);

    auto r = lt.find({"inner0", "pi"});
    CHECK(r);
    a.write(*r, gfx::pi);

    auto n = lt.find({"inner0", "normal"});
    CHECK(n);
    a.write(*n, testVec3);
  }

  {
    LayoutRef dynRef = rtRef;
    auto dynStructLayout = lt.structLookup.at(innerSt1);
    LayoutTraverser ltInner(dynStructLayout, structMap);

    std::optional<LayoutRef> normal0Inner = ltInner.find({"normal0"});
    CHECK(normal0Inner);
    std::optional<LayoutRef> normal1Inner = ltInner.find({"normal1"});
    CHECK(normal1Inner);

    for (size_t i = 0; i < elementCount; i++) {
      size_t dynOffset = rtRef.offset + i * rtRef.size;

      dynRef.type = normal0Inner->type;
      dynRef.size = normal0Inner->size;
      dynRef.offset = dynOffset + normal0Inner->offset;
      a.write(dynRef, float(i));

      dynRef.type = normal1Inner->type;
      dynRef.size = normal1Inner->size;
      dynRef.offset = dynOffset + normal1Inner->offset;
      a.write(dynRef, i);
    }
  }

  // Read data back and check
  {
    auto f = lt.find({"inner0", "four"});
    CHECK(f);
    int i{};
    a.read(*f, i);
    CHECK(i == 4);

    auto r = lt.find({"inner0", "pi"});
    CHECK(r);
    float pi;
    a.read(*r, pi);
    CHECK(pi == gfx::pi);

    auto n = lt.find({"inner0", "normal"});
    CHECK(n);
    float3 vec;
    a.read(*n, vec);
    CHECK(vec == testVec3);
  }

  {
    LayoutRef dynRef = rtRef;
    auto dynStructLayout = lt.structLookup.at(innerSt1);
    LayoutTraverser ltInner(dynStructLayout, structMap);

    std::optional<LayoutRef> normal0Inner = ltInner.find({"normal0"});
    CHECK(normal0Inner);
    std::optional<LayoutRef> normal1Inner = ltInner.find({"normal1"});
    CHECK(normal1Inner);

    for (size_t i = 0; i < elementCount; i++) {
      size_t dynOffset = rtRef.offset + i * rtRef.size;

      dynRef.type = normal0Inner->type;
      dynRef.size = normal0Inner->size;
      dynRef.offset = dynOffset + normal0Inner->offset;
      float f{};
      a.read(dynRef, f);
      CHECK(f == float(i));

      dynRef.type = normal1Inner->type;
      dynRef.size = normal1Inner->size;
      dynRef.offset = dynOffset + normal1Inner->offset;
      int i1;
      a.read(dynRef, i1);
      CHECK(i1 == i);
    }
  }
}

#include <gfx/shader/layout_traverser2.hpp>

TEST_CASE("LT2", "[Shader]") {
  StructType innerSt1;
  innerSt1->addField("normal0", Types::Float);
  innerSt1->addField("normal1", Types::Int32);
  StructType st;
  st->addField("inner1", ArrayType(innerSt1, 4));

  StructLayoutBuilder lb(shader::AddressSpace::StorageRW);
  lb.pushFromStruct(st);
  auto layout = lb.finalize();
  auto structLookup = lb.getStructMap();

  shader::LayoutTraverser2 lt(layout, structLookup, shader::AddressSpace::StorageRW);
  auto r = lt.root();
  CHECK(r.size() == 8 * 4);

  auto i = r.inner("inner1");
  CHECK(i);
  CHECK(i->type() == Type(ArrayType(innerSt1, 4)));

  auto innerStruct = i->inner();
  CHECK(innerStruct.offset == 0);
  CHECK(innerStruct.size() == 8);

  auto n1 = innerStruct.inner("normal1");
  CHECK(n1);
  CHECK(n1->offset == 4);
  CHECK(n1->type() == Types::Int32);
  CHECK(n1->size() == 4);
}