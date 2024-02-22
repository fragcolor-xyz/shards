#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/common_types.hpp>
#include <shards/core/exposed_type_utils.hpp>
#include <shards/core/object_var_util.hpp>

#include "debug.hpp"
#include "log.hpp"
#include "many_ui.hpp"
#include "repr.hpp"

static auto logger = ui2::getLogger();

using namespace shards;
namespace ui2 {

struct Context {
  static constexpr uint32_t TypeId = 'uict';
  static inline SHTypeInfo Type{SHType::Object, {.object = {.vendorId = CoreCC, .typeId = TypeId}}};
  static inline const char VariableName[] = "UI.Context";
  static inline const SHOptionalString VariableDescription = SHCCSTR("The UI context.");
  static inline SHExposedTypeInfo VariableInfo = shards::ExposedInfo::ProtectedVariable(VariableName, VariableDescription, Type);

  repr::Root repr;
  bool isModified{};

  template <typename C> void enterNode(repr::Node::Ptr node, C &&callback) {
    node->enter(repr);
    callback();
    node->leave(repr);
  }
};

using RequiredUIContext = RequiredContextVariable<Context, Context::Type, Context::VariableName, true>;

struct NodeShard {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR(""); }

  PARAM(ShardsVar, _contents, "Contents", "", {shards::CoreInfo::ShardsOrNoneSeq});
  PARAM_PARAMVAR(_payload, "Payload", "Test", {shards::CoreInfo::AnyType});
  PARAM_IMPL(PARAM_IMPL_FOR(_contents), PARAM_IMPL_FOR(_payload));

  // repr::Node::Ptr _node = repr::Node::create();

  RequiredUIContext _context;
  ManyUI<repr::Node::Ptr, []() { return repr::Node::create(); }> _nodeStorage;

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _context.warmup(context);
  }
  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    _context.cleanup(context);
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    _context.compose(data, _requiredVariables);

    _contents.compose(data);
    for (auto &r : _contents.composeResult().requiredInfo)
      _requiredVariables.push_back(r);

    return outputTypes().elements[0];
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto node = _nodeStorage.pull(_context->repr.currentVersion);

#if SHARDS_UI_LABELS
    node->label = fmt::format("{}", _payload.get());
#endif
    _context->enterNode(node, [&] {
      if (_contents) {
        SHVar output{};
        _contents.activate(shContext, input, output); //
      }
    });
    return input;
  }
};

struct ContextShard {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR(""); }

  PARAM(ShardsVar, _contents, "Contents", "", {shards::CoreInfo::ShardsOrNoneSeq});
  PARAM_VAR(_debug, "Debug", "", {shards::CoreInfo::BoolType});
  PARAM_IMPL(PARAM_IMPL_FOR(_contents), PARAM_IMPL_FOR(_debug));

  SHVar *_contextVarRef{};
  Context _context;

  ContextShard() { _debug = Var(false); }

  void warmup(SHContext *context) {
    _contextVarRef = referenceVariable(context, Context::VariableName);
    withObjectVariable(*_contextVarRef, &_context, Context::Type, [&] { PARAM_WARMUP(context); });
  }

  void cleanup(SHContext *context) {
    if (_contextVarRef) {
      withObjectVariable(*_contextVarRef, &_context, Context::Type, [&] { PARAM_CLEANUP(context); });
      releaseVariable(_contextVarRef);
      _contextVarRef = nullptr;
    }
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    ExposedInfo innerExposedInfo = ExposedInfo(data.shared);
    innerExposedInfo.push_back(Context::VariableInfo);

    SHInstanceData contentInstanceData = data;
    contentInstanceData.shared = SHExposedTypesInfo(innerExposedInfo);

    _contents.compose(contentInstanceData);
    for (auto &r : _contents.composeResult().requiredInfo) {
      if (std::string_view(r.name) == Context::VariableName)
        continue;
      _requiredVariables.push_back(r);
    }

    return _contents.composeResult().outputType;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    SHVar output{};
    withObjectVariable(*_contextVarRef, &_context, Context::Type, [&] {
      auto &rootRepr = _context.repr;
#if SHARDS_UI_LABELS
      rootRepr.container->label = "<root>";
#endif
      rootRepr.enter();
      _contents.activate(shContext, input, output);
      rootRepr.leave();
    });
    if ((bool)*_debug) {
      std::stringstream str;
      repr::debugDump(str, _context.repr.container);
      SPDLOG_LOGGER_INFO(logger, "UI:\n{}", str.str());
    }
    return output;
  }
};

} // namespace ui2

SHARDS_REGISTER_FN(test) {
  using namespace ui2;
  REGISTER_SHARD("UI", ContextShard);
  REGISTER_SHARD("UI.Node", NodeShard);
}