#ifndef GFX_SHADER_SHARDS
#define GFX_SHADER_SHARDS

#include "shard.hpp"
#include "generator.hpp"
#include <string>
#include <type_traits>

namespace gfx {
namespace shader {

namespace shards {

struct Compound : public Shard {
  std::vector<UniquePtr<Shard>> children;

  template <typename... TArgs> Compound(TArgs... args) { (..., children.push_back(ConvertToShard<TArgs>{}(std::move(args)))); }

  void apply(GeneratorContext &context) const {
    for (auto &c : children)
      c->apply(context);
  }

  ShardPtr clone() {
    auto result = std::make_unique<Compound>();
    for (auto &child : children)
      result->children.push_back(child->clone());
    return std::move(result);
  }
};
template <typename... TArgs> inline auto makeCompoundShard(TArgs &&...args) {
  return std::make_unique<Compound>(std::forward<TArgs>(args)...);
}

struct WithInput : public Shard {
  String name;
  ShardPtr inner;
  ShardPtr innerElse;

  template <typename T>
  WithInput(const String &name, T &&inner) : name(name), inner(ConvertToShard<T>{}(std::forward<T>(inner))) {}
  template <typename T1, typename T2>
  WithInput(const String &name, T1 &&inner, T2 &&innerElse)
      : name(name), inner(ConvertToShard<T1>{}(std::forward<T1>(inner))),
        innerElse(ConvertToShard<T2>{}(std::forward<T2>(innerElse))) {}
  WithInput(WithInput &&other) = default;

  void apply(GeneratorContext &context) const {
    if (context.hasInput(name.c_str())) {
      inner->apply(context);
    } else if (innerElse) {
      innerElse->apply(context);
    }
  }

  ShardPtr clone() {
    if (innerElse)
      return std::make_unique<WithInput>(name, inner->clone(), innerElse->clone());
    else
      return std::make_unique<WithInput>(name, inner->clone());
  }
};

struct WithTexture : public Shard {
  String name;
  ShardPtr inner;
  ShardPtr innerElse;

  template <typename T>
  WithTexture(const String &name, T &&inner) : name(name), inner(ConvertToShard<T>{}(std::forward<T>(inner))) {}
  template <typename T1, typename T2>
  WithTexture(const String &name, T1 &&inner, T2 &&innerElse)
      : name(name), inner(ConvertToShard<T1>{}(std::forward<T1>(inner))),
        innerElse(ConvertToShard<T2>{}(std::forward<T2>(innerElse))) {}
  WithTexture(WithTexture &&other) = default;

  void apply(GeneratorContext &context) const {
    if (context.hasTexture(name.c_str())) {
      inner->apply(context);
    } else if (innerElse) {
      innerElse->apply(context);
    }
  }

  ShardPtr clone() {
    if (innerElse)
      return std::make_unique<WithTexture>(name, inner->clone(), innerElse->clone());
    else
      return std::make_unique<WithTexture>(name, inner->clone());
  }
};

struct WithOutput : public Shard {
  String name;
  ShardPtr inner;
  ShardPtr innerElse;

  template <typename T>
  WithOutput(const String &name, T &&inner) : name(name), inner(ConvertToShard<T>{}(std::forward<T>(inner))) {}
  template <typename T1, typename T2>
  WithOutput(const String &name, T1 &&inner, T2 &&innerElse)
      : name(name), inner(ConvertToShard<T1>{}(std::forward<T1>(inner))),
        innerElse(ConvertToShard<T2>{}(std::forward<T2>(innerElse))) {}
  WithOutput(WithOutput &&other) = default;

  void apply(GeneratorContext &context) const {
    if (context.hasOutput(name.c_str())) {
      inner->apply(context);
    } else if (innerElse) {
      innerElse->apply(context);
    }
  }

  ShardPtr clone() {
    if (innerElse)
      return std::make_unique<WithInput>(name, inner->clone(), innerElse->clone());
    else
      return std::make_unique<WithInput>(name, inner->clone());
  }
};

struct WriteOutput : public Shard {
  String name;
  FieldType type;
  ShardPtr inner;

  template <typename T>
  WriteOutput(const String &name, FieldType type, T &&inner)
      : name(name), type(type), inner(ConvertToShard<T>{}(std::forward<T>(inner))) {}
  template <typename... TArgs>
  WriteOutput(const String &name, FieldType type, TArgs &&...inner)
      : name(name), type(type), inner(makeCompoundShard(std::forward<TArgs>(inner)...)) {}
  WriteOutput(WriteOutput &&other) = default;

  void apply(GeneratorContext &context) const {
    context.writeOutput(name.c_str(), type);
    context.write(" = ");
    inner->apply(context);
    context.write(";\n");
  }

  ShardPtr clone() { return std::make_unique<WriteOutput>(name, type, inner->clone()); }
};

struct ReadInput : public Shard {
  String name;

  ReadInput(const String &name) : name(name) {}
  ReadInput(ReadInput &&other) = default;

  void apply(GeneratorContext &context) const { context.readInput(name.c_str()); }

  ShardPtr clone() { return std::make_unique<ReadInput>(name); }
};

struct WriteGlobal : public Shard {
  String name;
  FieldType type;
  ShardPtr inner;

  template <typename T>
  WriteGlobal(const String &name, FieldType type, T &&inner)
      : name(name), type(type), inner(ConvertToShard<T>{}(std::forward<T>(inner))) {}
  template <typename... TArgs>
  WriteGlobal(const String &name, FieldType type, TArgs &&...inner)
      : name(name), type(type), inner(makeCompoundShard(std::forward<TArgs>(inner)...)) {}
  WriteGlobal(WriteGlobal &&other) = default;

  void apply(GeneratorContext &context) const {
    context.writeGlobal(name.c_str(), type);
    context.write(" = ");
    inner->apply(context);
    context.write(";\n");
  }

  ShardPtr clone() { return std::make_unique<WriteGlobal>(name, type, inner->clone()); }
};

struct ReadGlobal : public Shard {
  String name;

  ReadGlobal(const String &name) : name(name) {}
  ReadGlobal(ReadGlobal &&other) = default;

  void apply(GeneratorContext &context) const { context.readGlobal(name.c_str()); }

  ShardPtr clone() { return std::make_unique<ReadGlobal>(name); }
};

struct ReadBuffer : public Shard {
  String name;

  ReadBuffer(const String &name) : name(name) {}
  ReadBuffer(ReadBuffer &&other) = default;

  void apply(GeneratorContext &context) const { context.readBuffer(name.c_str()); }

  ShardPtr clone() { return std::make_unique<ReadBuffer>(name); }
};

struct SampleTexture : public Shard {
  String name;
  ShardPtr sampleCoordinate;

  SampleTexture(const String &name) : name(name) {}
  template <typename T>
  SampleTexture(const String &name, T &&sampleCoordinate)
      : name(name), sampleCoordinate(ConvertToShard<T>{}(std::forward<T>(sampleCoordinate))) {}
  SampleTexture(SampleTexture &&other) = default;

  void apply(GeneratorContext &context) const {
    context.write("textureSample(");
    context.texture(name.c_str());
    context.write(", ");
    context.textureDefaultSampler(name.c_str());
    context.write(", ");
    if (sampleCoordinate) {
      sampleCoordinate->apply(context);
    } else {
      context.textureDefaultTextureCoordinate(name.c_str());
    }
    context.write(")");
  }

  ShardPtr clone() { return std::make_unique<SampleTexture>(name, sampleCoordinate); }
};

// Runs callback at code generation time
struct Custom : public Shard {
  typedef std::function<void(GeneratorContext &context)> Callback;
  Callback callback;

  Custom(Callback &&callback) : callback(std::forward<Callback>(callback)) {}
  Custom(Custom &&other) = default;
  Custom(const Custom &other) = default;

  void apply(GeneratorContext &context) const { callback(context); }

  ShardPtr clone() { return std::make_unique<Custom>(*this); }
};

// Generates passthrough outputs for each input if an output is not already written to
// this is only applied for inputs that match matchPrefixes
struct DefaultInterpolation : public Shard {
  std::vector<String> matchPrefixes;

  DefaultInterpolation();
  void apply(GeneratorContext &context) const;
  ShardPtr clone() { return std::make_unique<DefaultInterpolation>(*this); }
};

template <typename T> inline auto toShard(T &&arg) { return ConvertibleToShard(std::forward<T>(arg)); }
template <typename T, typename... TArgs> inline auto makeShard(TArgs &&...args) {
  return std::make_unique<T>(std::forward<TArgs>(args)...);
}

} // namespace shards
} // namespace shader
} // namespace gfx

#endif // GFX_SHADER_SHARDS
