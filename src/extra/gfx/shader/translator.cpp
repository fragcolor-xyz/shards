#include "translator.hpp"
#include "core_shards.hpp"
#include "linalg_shards.hpp"
#include "math_shards.hpp"
#include "spdlog/spdlog.h"
#include <common_types.hpp>
#include <foundation.hpp>
#include <gfx/shader/generator.hpp>

using namespace gfx::shader;
using namespace shards;
namespace gfx {
namespace shader {
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
  // Literal copy-paste into shader code
  REGISTER_SHADER_SHARD("Shader.Literal", Literal);
  REGISTER_SHADER_SHARD("Shader.ReadInput", gfx::shader::Read<blocks::ReadInput>);
  REGISTER_SHADER_SHARD("Shader.ReadBuffer", gfx::shader::ReadBuffer);
  REGISTER_SHADER_SHARD("Shader.ReadGlobal", gfx::shader::Read<blocks::ReadGlobal>);
  REGISTER_SHADER_SHARD("Shader.WriteGlobal", gfx::shader::Write<blocks::WriteGlobal>);
  REGISTER_SHADER_SHARD("Shader.WriteOutput", gfx::shader::Write<blocks::WriteOutput>);

  // Math blocks
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "Math.Add", shards::Math::Add, OperatorAdd);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "Math.Subtract", shards::Math::Subtract, OperatorSubtract);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "Math.Multiply", shards::Math::Multiply, OperatorMultiply);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "Math.Divide", shards::Math::Divide, OperatorDivide);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "Math.Mod", shards::Math::Mod, OperatorMod);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Cos", shards::Math::Cos, OperatorCos);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Sin", shards::Math::Sin, OperatorSin);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Tan", shards::Math::Tan, OperatorTan);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "Max", shards::Math::Max, OperatorMax);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "Min", shards::Math::Min, OperatorMin);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "Math.Pow", shards::Math::Pow, OperatorPow);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Log", shards::Math::Log, OperatorLog);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Exp", shards::Math::Exp, OperatorExp);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Floor", shards::Math::Floor, OperatorFloor);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Ceil", shards::Math::Ceil, OperatorCeil);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Round", shards::Math::Round, OperatorRound);

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
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "Math.MatMul", LinAlg::MatMul, OperatorMatMul);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "Math.Dot", LinAlg::Dot, OperatorDot);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(BinaryOperatorTranslator, "Math.Cross", LinAlg::Cross, OperatorCross);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Normalize", LinAlg::Normalize, OperatorNormalize);
  REGISTER_EXTERNAL_SHADER_SHARD_T2(UnaryOperatorTranslator, "Math.Length", LinAlg::Length, OperatorLength);

  REGISTER_EXTERNAL_SHADER_SHARD(ConstTranslator, "Const", shards::Const);
  REGISTER_EXTERNAL_SHADER_SHARD(SetTranslator, "Set", shards::Set);
  REGISTER_EXTERNAL_SHADER_SHARD(GetTranslator, "Get", shards::Get);
  REGISTER_EXTERNAL_SHADER_SHARD(UpdateTranslator, "Update", shards::Update);
  REGISTER_EXTERNAL_SHADER_SHARD(TakeTranslator, "Take", shards::Take);
}
} // namespace shader
} // namespace gfx
