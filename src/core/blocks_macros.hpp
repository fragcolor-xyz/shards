#define RUNTIME_BLOCK(_typename_, _namespace_, _name_) namespace _namespace_ {\
  struct _name_##Runtime {\
    CBRuntimeBlock header;\
    _typename_ core;\
  };\
  __cdecl CBRuntimeBlock* createBlock##_name_() {\
    CBRuntimeBlock* result = reinterpret_cast<CBRuntimeBlock*>(new _name_##Runtime());\
    result->name = static_cast<CBNameProc>([] (CBRuntimeBlock* block) { return #_namespace_ "." #_name_ ; });\
    result->help = static_cast<CBHelpProc>([] (CBRuntimeBlock* block) { return ""; });\
    result->setup = static_cast<CBSetupProc>([] (CBRuntimeBlock* block) {});\
    result->destroy = static_cast<CBDestroyProc>([] (CBRuntimeBlock* block) {});\
    result->inputTypes = static_cast<CBInputTypesProc>([] (CBRuntimeBlock* block) { return CBTypesInfo(); });\
    result->outputTypes = static_cast<CBOutputTypesProc>([] (CBRuntimeBlock* block) { return CBTypesInfo(); });\
    result->exposedVariables = static_cast<CBExposedVariablesProc>([] (CBRuntimeBlock* block) { return CBParametersInfo(); });\
    result->consumedVariables = static_cast<CBConsumedVariablesProc>([] (CBRuntimeBlock* block) { return CBParametersInfo(); });\
    result->parameters = static_cast<CBParametersProc>([] (CBRuntimeBlock* block) { return CBParametersInfo(); });\
    result->setParam = static_cast<CBSetParamProc>([] (CBRuntimeBlock* block, int index, CBVar value) {});\
    result->getParam = static_cast<CBGetParamProc>([] (CBRuntimeBlock* block, int index) { return CBVar(); });\
    result->preChain = nullptr;\
    result->postChain = nullptr;\
    result->cleanup = static_cast<CBCleanupProc>([] (CBRuntimeBlock* block) {});

#define RUNTIME_CORE_BLOCK(_typename_, _name_) struct _name_##Runtime {\
    CBRuntimeBlock header;\
    _typename_ core;\
  };\
  __cdecl CBRuntimeBlock* createBlock##_name_() {\
    CBRuntimeBlock* result = reinterpret_cast<CBRuntimeBlock*>(new _name_##Runtime());\
    result->name = static_cast<CBNameProc>([] (CBRuntimeBlock* block) { return #_name_ ; });\
    result->help = static_cast<CBHelpProc>([] (CBRuntimeBlock* block) { return ""; });\
    result->setup = static_cast<CBSetupProc>([] (CBRuntimeBlock* block) {});\
    result->destroy = static_cast<CBDestroyProc>([] (CBRuntimeBlock* block) {});\
    result->inputTypes = static_cast<CBInputTypesProc>([] (CBRuntimeBlock* block) { return CBTypesInfo(); });\
    result->outputTypes = static_cast<CBOutputTypesProc>([] (CBRuntimeBlock* block) { return CBTypesInfo(); });\
    result->exposedVariables = static_cast<CBExposedVariablesProc>([] (CBRuntimeBlock* block) { return CBParametersInfo(); });\
    result->consumedVariables = static_cast<CBConsumedVariablesProc>([] (CBRuntimeBlock* block) { return CBParametersInfo(); });\
    result->parameters = static_cast<CBParametersProc>([] (CBRuntimeBlock* block) { return CBParametersInfo(); });\
    result->setParam = static_cast<CBSetParamProc>([] (CBRuntimeBlock* block, int index, CBVar value) {});\
    result->getParam = static_cast<CBGetParamProc>([] (CBRuntimeBlock* block, int index) { return CBVar(); });\
    result->preChain = nullptr;\
    result->postChain = nullptr;\
    result->cleanup = static_cast<CBCleanupProc>([] (CBRuntimeBlock* block) {});

// Those get nicely inlined fully so only 1 indirection will happen at the root of the call if the block is all inline
#define RUNTIME_BLOCK_name(_name_) result->name = static_cast<CBSetupProc>([] (CBRuntimeBlock* block) { return reinterpret_cast<_name_##Runtime*>(block)->core.name(); });
#define RUNTIME_BLOCK_help(_name_) result->help = static_cast<CBSetupProc>([] (CBRuntimeBlock* block) { return reinterpret_cast<_name_##Runtime*>(block)->core.help(); });

#define RUNTIME_BLOCK_setup(_name_) result->setup = static_cast<CBSetupProc>([] (CBRuntimeBlock* block) { reinterpret_cast<_name_##Runtime*>(block)->core.setup(); });
#define RUNTIME_BLOCK_destroy(_name_) result->destroy = static_cast<CBDestroyProc>([] (CBRuntimeBlock* block) { reinterpret_cast<_name_##Runtime*>(block)->core.destroy(); });

#define RUNTIME_BLOCK_inputTypes(_name_) result->inputTypes = static_cast<CBInputTypesProc>([] (CBRuntimeBlock* block) { return reinterpret_cast<_name_##Runtime*>(block)->core.inputTypes(); });
#define RUNTIME_BLOCK_outputTypes(_name_) result->outputTypes = static_cast<CBOutputTypesProc>([] (CBRuntimeBlock* block) { return reinterpret_cast<_name_##Runtime*>(block)->core.outputTypes(); });

#define RUNTIME_BLOCK_exposedVariables(_name_) result->exposedVariables = static_cast<CBExposedVariablesProc>([] (CBRuntimeBlock* block) { return reinterpret_cast<_name_##Runtime*>(block)->core.exposedVariables(); });
#define RUNTIME_BLOCK_consumedVariables(_name_) result->consumedVariables = static_cast<CBConsumedVariablesProc>([] (CBRuntimeBlock* block) { return reinterpret_cast<_name_##Runtime*>(block)->core.consumedVariables(); });

#define RUNTIME_BLOCK_parameters(_name_) result->parameters = static_cast<CBParametersProc>([] (CBRuntimeBlock* block) { return reinterpret_cast<_name_##Runtime*>(block)->core.parameters(); });
#define RUNTIME_BLOCK_setParam(_name_) result->setParam = static_cast<CBSetParamProc>([] (CBRuntimeBlock* block, int index, CBVar value) { reinterpret_cast<_name_##Runtime*>(block)->core.setParam(index, value); });
#define RUNTIME_BLOCK_getParam(_name_) result->getParam = static_cast<CBGetParamProc>([] (CBRuntimeBlock* block, int index) { return reinterpret_cast<_name_##Runtime*>(block)->core.getParam(index); });

#define RUNTIME_BLOCK_preChain(_name_) result->preChain = static_cast<CBPreChainProc>([] (CBRuntimeBlock* block, CBContext* context) { reinterpret_cast<_name_##Runtime*>(block)->core.preChain(context); });
#define RUNTIME_BLOCK_activate(_name_) result->activate = static_cast<CBActivateProc>([] (CBRuntimeBlock* block, CBContext* context, CBVar input) { return reinterpret_cast<_name_##Runtime*>(block)->core.activate(context, input); });
#define RUNTIME_BLOCK_postChain(_name_) result->postChain = static_cast<CBPostChainProc>([] (CBRuntimeBlock* block, CBContext* context) { reinterpret_cast<_name_##Runtime*>(block)->core.postChain(context); });

#define RUNTIME_BLOCK_cleanup(_name_) result->cleanup = static_cast<CBCleanupProc>([] (CBRuntimeBlock* block) { reinterpret_cast<_name_##Runtime*>(block)->core.cleanup(); });

#define RUNTIME_BLOCK_END(_name_) return result;\
  }

#define REGISTER_BLOCK(_namespace_, _name_) chainblocks::registerBlock(#_namespace_ "." #_name_, _namespace_::createBlock##_name_)
#define REGISTER_CORE_BLOCK(_name_) chainblocks::registerBlock(#_name_, createBlock##_name_)

