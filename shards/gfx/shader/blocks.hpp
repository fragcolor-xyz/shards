#ifndef GFX_SHADER_BLOCKS
#define GFX_SHADER_BLOCKS

#include "block.hpp"
#include "generator.hpp"
#include <string>
#include <type_traits>

namespace gfx {
namespace shader {

namespace blocks {

struct Compound : public Block {
  std::vector<UniquePtr<Block>> children;

  template <typename... TArgs> Compound(TArgs... args) { append<TArgs...>(std::forward<TArgs>(args)...); }

  template <typename... TArgs> void append(TArgs... args) { (..., children.push_back(ConvertToBlock<TArgs>{}(std::move(args)))); }
  template <typename... TArgs> void appendLine(TArgs... args) { append(std::forward<TArgs>(args)..., ";\n"); }

  void apply(IGeneratorContext &context) const {
    for (auto &c : children)
      c->apply(context);
  }

  BlockPtr clone() {
    auto result = std::make_unique<Compound>();
    for (auto &child : children)
      result->children.push_back(child->clone());
    return std::move(result);
  }
};
template <typename... TArgs> inline auto makeCompoundBlock(TArgs &&...args) {
  return std::make_unique<Compound>(std::forward<TArgs>(args)...);
}

struct WithInput : public Block {
  FastString name;
  BlockPtr inner;
  BlockPtr innerElse;

  template <typename T>
  WithInput(FastString name, T &&inner) : name(name), inner(ConvertToBlock<T>{}(std::forward<T>(inner))) {}
  template <typename T1, typename T2>
  WithInput(FastString name, T1 &&inner, T2 &&innerElse)
      : name(name), inner(ConvertToBlock<T1>{}(std::forward<T1>(inner))),
        innerElse(ConvertToBlock<T2>{}(std::forward<T2>(innerElse))) {}
  WithInput(WithInput &&other) = default;

  void apply(IGeneratorContext &context) const {
    if (context.hasInput(name)) {
      inner->apply(context);
    } else if (innerElse) {
      innerElse->apply(context);
    }
  }

  BlockPtr clone() {
    if (innerElse)
      return std::make_unique<WithInput>(name, inner->clone(), innerElse->clone());
    else
      return std::make_unique<WithInput>(name, inner->clone());
  }
};

struct WithTexture : public Block {
  FastString name;
  BlockPtr inner;
  BlockPtr innerElse;
  bool defaultTexcoordRequired = true;

  template <typename T>
  WithTexture(FastString name, bool defaultTexcoordRequired, T &&inner)
      : name(name), inner(ConvertToBlock<T>{}(std::forward<T>(inner))), defaultTexcoordRequired(defaultTexcoordRequired) {}
  template <typename T1, typename T2>
  WithTexture(FastString name, bool defaultTexcoordRequired, T1 &&inner, T2 &&innerElse)
      : name(name), inner(ConvertToBlock<T1>{}(std::forward<T1>(inner))),
        innerElse(ConvertToBlock<T2>{}(std::forward<T2>(innerElse))), defaultTexcoordRequired(defaultTexcoordRequired) {}
  WithTexture(WithTexture &&other) = default;

  void apply(IGeneratorContext &context) const {
    if (context.hasTexture(name)) {
      inner->apply(context);
    } else if (innerElse) {
      innerElse->apply(context);
    }
  }

  BlockPtr clone() {
    if (innerElse)
      return std::make_unique<WithTexture>(name, defaultTexcoordRequired, inner->clone(), innerElse->clone());
    else
      return std::make_unique<WithTexture>(name, defaultTexcoordRequired, inner->clone());
  }
};

struct WithOutput : public Block {
  FastString name;
  BlockPtr inner;
  BlockPtr innerElse;

  template <typename T>
  WithOutput(FastString name, T &&inner) : name(name), inner(ConvertToBlock<T>{}(std::forward<T>(inner))) {}
  template <typename T1, typename T2>
  WithOutput(FastString name, T1 &&inner, T2 &&innerElse)
      : name(name), inner(ConvertToBlock<T1>{}(std::forward<T1>(inner))),
        innerElse(ConvertToBlock<T2>{}(std::forward<T2>(innerElse))) {}
  WithOutput(WithOutput &&other) = default;

  void apply(IGeneratorContext &context) const {
    if (context.hasOutput(name)) {
      inner->apply(context);
    } else if (innerElse) {
      innerElse->apply(context);
    }
  }

  BlockPtr clone() {
    if (innerElse)
      return std::make_unique<WithInput>(name, inner->clone(), innerElse->clone());
    else
      return std::make_unique<WithInput>(name, inner->clone());
  }
};

struct WriteOutput : public Block {
  FastString name;
  NumType type;
  BlockPtr inner;

  template <typename T>
  WriteOutput(FastString name, NumType type, T &&inner)
      : name(name), type(type), inner(ConvertToBlock<T>{}(std::forward<T>(inner))) {}
  template <typename... TArgs>
  WriteOutput(FastString name, NumType type, TArgs &&...inner)
      : name(name), type(type), inner(makeCompoundBlock(std::forward<TArgs>(inner)...)) {}
  WriteOutput(WriteOutput &&other) = default;

  void apply(IGeneratorContext &context) const {
    context.writeOutput(name, type);
    context.write(" = ");
    inner->apply(context);
    context.write(";\n");
  }

  BlockPtr clone() { return std::make_unique<WriteOutput>(name, type, inner->clone()); }
};

struct ReadInput : public Block {
  FastString name;

  ReadInput(FastString name) : name(name) {}
  ReadInput(ReadInput &&other) = default;

  void apply(IGeneratorContext &context) const { context.readInput(name); }

  BlockPtr clone() { return std::make_unique<ReadInput>(name); }
};

struct WriteGlobal : public Block {
  FastString name;
  NumType type;
  BlockPtr inner;

  template <typename T>
  WriteGlobal(FastString name, NumType type, T &&inner)
      : name(name), type(type), inner(ConvertToBlock<T>{}(std::forward<T>(inner))) {}
  template <typename... TArgs>
  WriteGlobal(FastString name, NumType type, TArgs &&...inner)
      : name(name), type(type), inner(makeCompoundBlock(std::forward<TArgs>(inner)...)) {}
  WriteGlobal(WriteGlobal &&other) = default;

  void apply(IGeneratorContext &context) const {
    context.writeGlobal(name, type, [&]() { inner->apply(context); });
  }

  BlockPtr clone() { return std::make_unique<WriteGlobal>(name, type, inner->clone()); }
};

struct ReadGlobal : public Block {
  FastString name;

  ReadGlobal(FastString name) : name(name) {}
  ReadGlobal(ReadGlobal &&other) = default;

  void apply(IGeneratorContext &context) const { context.readGlobal(name); }

  BlockPtr clone() { return std::make_unique<ReadGlobal>(name); }
};

struct ReadBuffer : public Block {
  FastString fieldName;
  NumType type;
  FastString bufferName;

  ReadBuffer(FastString fieldName, const NumType &type, FastString bufferName = "object")
      : fieldName(fieldName), type(type), bufferName(bufferName) {}
  ReadBuffer(ReadBuffer &&other) = default;

  void apply(IGeneratorContext &context) const { context.readBuffer(fieldName, type, bufferName); }

  BlockPtr clone() { return std::make_unique<ReadBuffer>(fieldName, type, bufferName); }
};

struct SampleTexture : public Block {
  FastString name;
  BlockPtr sampleCoordinate;

  SampleTexture(FastString name) : name(name) {}
  SampleTexture(FastString name, BlockPtr &&sampleCoordinate) : name(name), sampleCoordinate(std::move(sampleCoordinate)) {}

  template <typename T>
  SampleTexture(FastString name, T &&sampleCoordinate)
      : name(name), sampleCoordinate(ConvertToBlock<T>{}(std::forward<T>(sampleCoordinate))) {}

  SampleTexture(SampleTexture &&other) = default;

  void apply(IGeneratorContext &context) const {
    context.write("textureSample(");
    context.texture(name);
    context.write(", ");
    context.textureDefaultSampler(name);
    context.write(", ");
    if (sampleCoordinate) {
      sampleCoordinate->apply(context);
    } else {
      context.textureDefaultTextureCoordinate(name);
    }
    context.write(")");
  }

  BlockPtr clone() { return std::make_unique<SampleTexture>(name, sampleCoordinate); }
};

struct LinearizeDepth : public Block {
  BlockPtr input;

  LinearizeDepth(BlockPtr &&input) : input(std::move(input)) {}

  void apply(IGeneratorContext &context) const {
    // Based on the linalg::frustum_matrix definition with
    // forward = neg_z & range = zero_to_one
    //
    // range = far - near
    // a = -far / range
    // b = -near * far / range
    // clip_depth = -a - b / z
    auto funcName = context.generateTempVariable();
    context.pushHeaderScope();
    context.write(fmt::format("fn {}(proj: mat4x4<f32>, clip_depth: f32) -> f32", funcName) + R"({
  let a = proj[2][2];
  let b = proj[3][2];
  return b / (clip_depth + a);
}
)");
    context.popHeaderScope();

    context.write(fmt::format("{}(", funcName));
    context.readBuffer("proj", Types::Float4x4, "view");
    context.write(", ");
    input->apply(context);
    context.write(")");
  }

  BlockPtr clone() { return std::make_unique<LinearizeDepth>(input->clone()); }
};

// Runs callback at code generation time
struct Custom : public Block {
  typedef std::function<void(IGeneratorContext &context)> Callback;
  Callback callback;

  Custom(Callback &&callback) : callback(std::forward<Callback>(callback)) {}
  Custom(Custom &&other) = default;
  Custom(const Custom &other) = default;

  void apply(IGeneratorContext &context) const { callback(context); }

  BlockPtr clone() { return std::make_unique<Custom>(*this); }
};

// Generates passthrough outputs for each input if an output is not already written to
// this is only applied for inputs that match matchPrefixes
struct DefaultInterpolation : public Block {
  std::vector<String> matchPrefixes;

  DefaultInterpolation();
  void apply(IGeneratorContext &context) const;
  BlockPtr clone() { return std::make_unique<DefaultInterpolation>(*this); }
};

template <typename T> inline auto toBlock(T &&arg) { return ConvertibleToBlock(std::forward<T>(arg)); }
template <typename T, typename... TArgs> inline auto makeBlock(TArgs &&...args) {
  return std::make_unique<T>(std::forward<TArgs>(args)...);
}

} // namespace blocks
} // namespace shader
} // namespace gfx

#endif // GFX_SHADER_BLOCKS
