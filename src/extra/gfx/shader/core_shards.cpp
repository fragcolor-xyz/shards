#include "core_shards.hpp"
#include "translate_wrapper.hpp"
#include <gfx/shader/fmt.hpp>

using namespace shards;
namespace gfx::shader {

struct PushTranslator {
  static void translate(Push *shard, TranslationContext &context) {
    auto &ref = context.getTop();

    auto value = context.takeWGSLTop();
    if (!value)
      throw std::runtime_error("Missing value to push");

    auto elementType = value->getType();

    auto it = ref.virtualSequences.find(shard->_name);
    if (it == ref.virtualSequences.end()) {
      it = ref.virtualSequences.emplace(std::make_pair(shard->_name, VirtualSeq())).first;
      it->second.elementType = elementType;
    } else {
      if (it->second.elementType != elementType)
        throw std::runtime_error(
            fmt::format("Sequence value type mismatch {} (old) != {} (new)", it->second.elementType, elementType));
    }

    auto &virtualSeq = it->second;
    virtualSeq.elements.emplace_back(std::make_unique<WGSLBlock>(elementType, value->toBlock()));

    // Restore / passthrough
    context.wgslTop = std::move(value);
  }
};

struct ForRangeTranslator {
  static void translate(ForRangeShard *shard, TranslationContext &context) {
    auto from = translateParamVar(shard->_from, context);
    auto to = translateParamVar(shard->_to, context);

    std::string indexVarName = context.getUniqueVariableName("index");

    context.enterNew(blocks::makeCompoundBlock());
    context.addNew(blocks::makeBlock<blocks::Direct>(fmt::format("for(int {} = ", indexVarName)));
    context.addNew(from->toBlock());
    context.addNew(blocks::makeBlock<blocks::Direct>(fmt::format("; {} < ", indexVarName)));
    context.addNew(to->toBlock());
    context.addNew(blocks::makeBlock<blocks::Direct>(fmt::format("; {}++) {", indexVarName)));

    // Pass index as input
    context.setWGSLTop<WGSLBlock>(FieldTypes::Int32, blocks::makeBlock<blocks::Direct>(indexVarName));

    // Loop body
    auto &shards = shard->_shards.shards();
    for (size_t i = 0; i < shards.len; i++) {
      context.processShard(shards.elements[i]);
    }

    context.addNew(blocks::makeBlock<blocks::Direct>("}"));
    context.leave();
  }
};

void registerCoreShards() {
  // Literal copy-paste into shader code
  REGISTER_SHADER_SHARD("Shader.Literal", Literal);
  REGISTER_SHADER_SHARD("Shader.ReadInput", gfx::shader::Read<blocks::ReadInput>);
  REGISTER_SHADER_SHARD("Shader.ReadBuffer", gfx::shader::ReadBuffer);
  REGISTER_SHADER_SHARD("Shader.ReadGlobal", gfx::shader::Read<blocks::ReadGlobal>);
  REGISTER_SHADER_SHARD("Shader.WriteGlobal", gfx::shader::Write<blocks::WriteGlobal>);
  REGISTER_SHADER_SHARD("Shader.WriteOutput", gfx::shader::Write<blocks::WriteOutput>);
  REGISTER_SHADER_SHARD("Shader.SampleTexture", gfx::shader::SampleTexture);
  REGISTER_SHADER_SHARD("Shader.SampleTextureUV", gfx::shader::SampleTextureUV);
  REGISTER_SHADER_SHARD("Shader.LinearizeDepth", gfx::shader::LinearizeDepth);

  REGISTER_EXTERNAL_SHADER_SHARD(ConstTranslator, "Const", shards::Const);
  REGISTER_EXTERNAL_SHADER_SHARD_T1(SetTranslator, "Set", shards::Set);
  REGISTER_EXTERNAL_SHADER_SHARD_T1(SetTranslator, "Ref", shards::Ref);
  REGISTER_EXTERNAL_SHADER_SHARD(GetTranslator, "Get", shards::Get);
  REGISTER_EXTERNAL_SHADER_SHARD(UpdateTranslator, "Update", shards::Update);
  REGISTER_EXTERNAL_SHADER_SHARD(TakeTranslator, "Take", shards::Take);

  REGISTER_EXTERNAL_SHADER_SHARD(PushTranslator, "Push", shards::Push);

  REGISTER_EXTERNAL_SHADER_SHARD(ForRangeTranslator, "ForRange", shards::ForRangeShard);
}
} // namespace gfx::shader