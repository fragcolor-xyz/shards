#include "translator.hpp"
#include "core_blocks.hpp"
#include "linalg_blocks.hpp"
#include "math_blocks.hpp"
#include "spdlog/spdlog.h"
#include <common_types.hpp>
#include <foundation.hpp>
#include <gfx/shader/generator.hpp>

using namespace gfx::shader;
using namespace shards;
namespace gfx {
namespace shader {
struct TestTranslator {
  ShardsVar shards{};

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }

  static SHParametersInfo parameters() {
    static Parameters params{
        {"Shards", SHCCSTR("The shader shards"), {CoreInfo::Shards}},
    };
    return params;
  }

  void setParam(int index, const SHVar &value) { shards = value; }
  SHVar getParam(int index) { return shards; }

  void cleanup() { shards.cleanup(); }

  void warmup(SHContext *ctx) { shards.warmup(ctx); }

  SHTypeInfo compose(const SHInstanceData &data) {
    shards.compose(data);
    return CoreInfo::StringType;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    SPDLOG_INFO("TestTranslator activation");

    TranslationContext context{};

    // Loop over shards passed as parameters
    // this recurses into child shards
    for (SHVar &shardVar : IterableSeq(SHVar(shards))) {
      context.processShard(shardVar.payload.shardValue);
    }

    // Placeholder to generate preview source code
    Generator shaderGenerator;
    {
      UniformBufferLayoutBuilder builder;
      builder.push("world", ShaderParamType::Float4x4);
      shaderGenerator.objectBufferLayout = builder.finalize();
    }
    {
      UniformBufferLayoutBuilder builder;
      builder.push("view", ShaderParamType::Float4x4);
      shaderGenerator.viewBufferLayout = builder.finalize();
    }
    shaderGenerator.meshFormat.vertexAttributes = {
        MeshVertexAttribute("position", 3),
        MeshVertexAttribute("normal", 3),
        MeshVertexAttribute("color", 4),
    };
    std::vector<EntryPoint> entryPoints;
    entryPoints.emplace_back("main", ProgrammableGraphicsStage::Vertex, std::move(context.root));
    GeneratorOutput generatorOutput = shaderGenerator.build(entryPoints);

    SPDLOG_INFO("Generated Shader: \n{}", generatorOutput.wgslSource);

    return SHVar{};
  }
};

static TranslationRegistry &getTranslationRegistry() {
  static TranslationRegistry instance;
  return instance;
}

TranslationContext::TranslationContext() : translationRegistry(getTranslationRegistry()), logger("ShaderTranslator") {
  logger.sinks() = spdlog::default_logger()->sinks();

  root = std::make_unique<blocks::Compound>();
  stack.push_back(TranslationBlockRef::make(root));
}

void TranslationContext::processShard(ShardPtr shard) {
  ITranslationHandler *handler = translationRegistry.resolve(shard);
  if (!handler) {
    throw ShaderComposeError(fmt::format("No shader translation available for shard {}", shard->name(shard)), shard);
  }

  handler->translate(shard, *this);
}

void TranslationRegistry::registerHandler(const char *blockName, ITranslationHandler *translateable) {
  handlers.insert_or_assign(blockName, translateable);
}

ITranslationHandler *TranslationRegistry::resolve(ShardPtr shard) {
  auto it = handlers.find((const char *)shard->name(shard));
  if (it != handlers.end())
    return it->second;
  return nullptr;
}

SH_HAS_MEMBER_TEST(translate);

template <typename TShard> struct TranslateWrapper : public ITranslationHandler {
  using ShardWrapper = shards::ShardWrapper<TShard>;

  void translate(ShardPtr shard, TranslationContext &context) {
    ShardWrapper *wrapper = reinterpret_cast<ShardWrapper *>(shard);
    TShard *innerShard = &wrapper->shard;

    static_assert(has_translate<TShard>::value, "Shader shards must have a \"translate\" method.");
    if constexpr (has_translate<TShard>::value) {
      innerShard->translate(context);
    }
  }
};

template <typename TShard> struct TranslateWrapperExternal : public ITranslationHandler {
  using ShardWrapper = shards::ShardWrapper<TShard>;
  using Callback = std::function<void(TShard *shard, TranslationContext &)>;

  Callback callback;

  TranslateWrapperExternal(Callback callback) : callback(callback){};
  void translate(ShardPtr shard, TranslationContext &context) {
    ShardWrapper *wrapper = reinterpret_cast<ShardWrapper *>(shard);
    TShard *innerShard = &wrapper->shard;

    callback(innerShard, context);
  }
};

// Translator is called as instance shard.translate(context&)
#define REGISTER_SHADER_SHARD(_name, _class)                    \
  {                                                             \
    static TranslateWrapper<_class> instance{};                 \
    getTranslationRegistry().registerHandler(_name, &instance); \
    REGISTER_SHARD(_name, _class);                              \
  }

// Translator is called as static TranslatorClass::translate(ShardClass*, context&)
#define REGISTER_EXTERNAL_SHADER_SHARD(_translatorClass, _name, _class)             \
  {                                                                                 \
    static TranslateWrapperExternal<_class> instance(&_translatorClass::translate); \
    getTranslationRegistry().registerHandler(_name, &instance);                     \
  }

// Translator is called as static TranslatorClass<TShard>::translate(TShard*, context&)
#define REGISTER_EXTERNAL_SHADER_SHARD_T1(_translatorClass, _name, _class)                  \
  {                                                                                         \
    static TranslateWrapperExternal<_class> instance(&_translatorClass<_class>::translate); \
    getTranslationRegistry().registerHandler(_name, &instance);                             \
  }

// Translator is called as static TranslatorClass<TShard, T1>::translate(TShard*, context&)
#define REGISTER_EXTERNAL_SHADER_SHARD_T2(_translatorClass, _name, _class, _t1)                  \
  {                                                                                              \
    static TranslateWrapperExternal<_class> instance(&_translatorClass<_class, _t1>::translate); \
    getTranslationRegistry().registerHandler(_name, &instance);                                  \
  }

namespace LinAlg {
using namespace shards::Math::LinAlg;
}
template <SHType ToType> using ToNumber = shards::ToNumber<ToType>;

void registerTranslatorShards() {
  REGISTER_SHARD("GFX.TestTranslator", TestTranslator);

  // Literal copy-paste into shader code
  REGISTER_SHADER_SHARD("Shader.Literal", Literal);
  REGISTER_SHADER_SHARD("Shader.ReadInput", gfx::shader::Read<blocks::ReadInput>);
  REGISTER_SHADER_SHARD("Shader.ReadBuffer", gfx::shader::ReadBuffer);
  REGISTER_SHADER_SHARD("Shader.ReadGlobal", gfx::shader::Read<blocks::ReadGlobal>);
  REGISTER_SHADER_SHARD("Shader.WriteGlobal", gfx::shader::Write<blocks::WriteGlobal>);
  REGISTER_SHADER_SHARD("Shader.WriteOutput", gfx::shader::Write<blocks::WriteOutput>);

  // Math shards
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "Math.Add", shards::Math::Add, OperatorAdd);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "Math.Subtract", shards::Math::Subtract, OperatorSubtract);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "Math.Multiply", shards::Math::Multiply, OperatorMultiply);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "Math.Divide", shards::Math::Divide, OperatorDivide);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "Math.Mod", shards::Math::Mod, OperatorMod);

  // Casting blocks
  REGISTER_EXTERNAL_SHADER_SHARD_T1(ToNumberTranslator, "ToInt", ToNumber<SHType::Int>);
  REGISTER_EXTERNAL_SHADER_SHARD_T1(ToNumberTranslator, "ToInt2", ToNumber<SHType::Int2>);
  REGISTER_EXTERNAL_SHADER_SHARD_T1(ToNumberTranslator, "ToInt3", ToNumber<SHType::Int3>);
  REGISTER_EXTERNAL_SHADER_SHARD_T1(ToNumberTranslator, "ToInt4", ToNumber<SHType::Int4>);
  REGISTER_EXTERNAL_SHADER_SHARD_T1(ToNumberTranslator, "ToInt8", ToNumber<SHType::Int8>);
  REGISTER_EXTERNAL_SHADER_SHARD_T1(ToNumberTranslator, "ToInt16", ToNumber<SHType::Int16>);
  REGISTER_EXTERNAL_SHADER_SHARD_T1(ToNumberTranslator, "ToColor", ToNumber<SHType::Color>);
  REGISTER_EXTERNAL_SHADER_SHARD_T1(ToNumberTranslator, "ToFloat", ToNumber<SHType::Float>);
  REGISTER_EXTERNAL_SHADER_SHARD_T1(ToNumberTranslator, "ToFloat2", ToNumber<SHType::Float2>);
  REGISTER_EXTERNAL_SHADER_SHARD_T1(ToNumberTranslator, "ToFloat3", ToNumber<SHType::Float3>);
  REGISTER_EXTERNAL_SHADER_SHARD_T1(ToNumberTranslator, "ToFloat4", ToNumber<SHType::Float4>);

  // Linalg blocks
  REGISTER_EXTERNAL_SHADER_SHARD(MatMulTranslator, "Math.MatMul", LinAlg::MatMul);

  REGISTER_EXTERNAL_SHADER_SHARD(ConstTranslator, "Const", shards::Const);
  REGISTER_EXTERNAL_SHADER_SHARD(SetTranslator, "Set", shards::Set);
  REGISTER_EXTERNAL_SHADER_SHARD(GetTranslator, "Get", shards::Get);
  REGISTER_EXTERNAL_SHADER_SHARD(UpdateTranslator, "Update", shards::Update);
  REGISTER_EXTERNAL_SHADER_SHARD(TakeTranslator, "Take", shards::Take);
}
} // namespace shader
} // namespace gfx
