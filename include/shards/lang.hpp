#ifndef C3497CAA_981D_4530_9234_3A93F96984D5
#define C3497CAA_981D_4530_9234_3A93F96984D5

#include "shards.h"
#include "utility.hpp"
#include <stdexcept>
#include <string_view>
#include <vector>

namespace shards::lang {
struct Exception : public std::exception {
  Exception(const SHLError &e) : message(e.message) {}
  const char *what() const noexcept override { return message.c_str(); }
  std::string message;
};

template <typename CORE> struct ReadEnvironment {
  std::vector<std::string> includePaths;

private:
  std::vector<SHStringWithLen> includePathsCache_;

public:
  ReadEnvironment() {}

  shards::TOwnedVar<CORE> read(std::string_view inputCode, std::string_view filename, std::string_view basePath) {
    includePathsCache_.clear();
    for (const auto &path : includePaths) {
      includePathsCache_.push_back(toSWL(path));
    }

    SHLAst ast{};
    DEFER(CORE::freeAst(&ast));
    if (!CORE::read(toSWL(filename), toSWL(inputCode), toSWL(basePath), includePathsCache_.data(), includePathsCache_.size(),
                    &ast))
      throw Exception(ast.error);
    return ast.ast;
  }

  static shards::TOwnedVar<CORE> loadAST(const uint8_t *bytes, uint32_t size) {
    SHLAst ast{};
    DEFER(CORE::freeAst(&ast));
    if (!CORE::loadAst(bytes, size, &ast))
      throw Exception(ast.error);
    return ast.ast;
  }
};

template <typename CORE> struct EvalEnvironment {
  SHLEvalEnv *env;

  EvalEnvironment(std::string_view namespace_ = {}) { CORE::createEvalEnv(toSWL(namespace_)); }
  ~EvalEnvironment() { reset(); }
  EvalEnvironment(EvalEnvironment &&other) noexcept : env(other.env) { other.env = nullptr; }
  EvalEnvironment &operator=(EvalEnvironment &&other) noexcept {
    env = other.env;
    other.env = nullptr;
    return *this;
  }
  EvalEnvironment(const EvalEnvironment &) = delete;
  EvalEnvironment &operator=(const EvalEnvironment &) = delete;
  void reset() {
    if (env != nullptr) {
      CORE::freeEvalEnv(env);
      env = nullptr;
    }
  }
  SHWireRef *eval(const shards::TOwnedVar<CORE> &ast, std::string_view name) {
    SHLWire wire;
    DEFER(CORE::freeWire(&wire));
    if (!CORE::evalAst(&(SHVar&)ast, toSWL(name), &wire))
      throw Exception(wire.error);
    return wire.wire;
  }
  // Transform environment into a wire, this consumes the environment
  static TOwnedVar<CORE> transformEnv(std::string_view name, EvalEnvironment &&env) {
    SHLWire wire;
    DEFER(CORE::freeWire(&wire));
    if (!CORE::transformEnv(env.env, toSWL(name), &wire))
      throw Exception(wire.error);

    TOwnedVar<CORE> v = SHVar{.payload = {.wireValue = *wire.wire}, .valueType = SHType::Wire};
    return v;
  }
};

} // namespace shards::lang

#endif /* C3497CAA_981D_4530_9234_3A93F96984D5 */
