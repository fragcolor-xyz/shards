#include "./context.hpp"
#include "gfx/enums.hpp"
#include "gfx/shader/log.hpp"
#include "gfx/shader/types.hpp"
#include <catch2/catch_all.hpp>
#include <cctype>
#include <gfx/context.hpp>
#include <gfx/shader/blocks.hpp>
#include <gfx/shader/generator.hpp>
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
