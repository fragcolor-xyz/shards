#define RUNTIME_BLOCK(_namespace_, _name_)                                     \
  struct _name_##Runtime {                                                     \
    CBlock header;                                                             \
    _name_ core;                                                               \
  };                                                                           \
  __cdecl CBlock *createBlock##_name_() {                                      \
    CBlock *result = reinterpret_cast<CBlock *>(new _name_##Runtime());        \
    result->name = static_cast<CBNameProc>(                                    \
        [](CBlock *block) { return #_namespace_ "." #_name_; });               \
    result->help = static_cast<CBHelpProc>([](CBlock *block) { return ""; });  \
    result->setup = static_cast<CBSetupProc>([](CBlock *block) {});            \
    result->destroy =                                                          \
        static_cast<CBDestroyProc>([](CBlock *block) { delete block; });       \
    result->inputTypes = static_cast<CBInputTypesProc>(                        \
        [](CBlock *block) { return CBTypesInfo(); });                          \
    result->outputTypes = static_cast<CBOutputTypesProc>(                      \
        [](CBlock *block) { return CBTypesInfo(); });                          \
    result->exposedVariables = static_cast<CBExposedVariablesProc>(            \
        [](CBlock *block) { return CBExposedTypesInfo(); });                   \
    result->consumedVariables = static_cast<CBConsumedVariablesProc>(          \
        [](CBlock *block) { return CBExposedTypesInfo(); });                   \
    result->parameters = static_cast<CBParametersProc>(                        \
        [](CBlock *block) { return CBParametersInfo(); });                     \
    result->setParam = static_cast<CBSetParamProc>(                            \
        [](CBlock *block, int index, CBVar value) {});                         \
    result->getParam = static_cast<CBGetParamProc>(                            \
        [](CBlock *block, int index) { return CBVar(); });                     \
    result->inferTypes = nullptr;                                              \
    result->cleanup = static_cast<CBCleanupProc>([](CBlock *block) {});

#define RUNTIME_CORE_BLOCK(_name_)                                             \
  struct _name_##Runtime {                                                     \
    CBlock header;                                                             \
    _name_ core;                                                               \
  };                                                                           \
  __cdecl CBlock *createBlock##_name_() {                                      \
    CBlock *result = reinterpret_cast<CBlock *>(new _name_##Runtime());        \
    result->name =                                                             \
        static_cast<CBNameProc>([](CBlock *block) { return #_name_; });        \
    result->help = static_cast<CBHelpProc>([](CBlock *block) { return ""; });  \
    result->setup = static_cast<CBSetupProc>([](CBlock *block) {});            \
    result->destroy =                                                          \
        static_cast<CBDestroyProc>([](CBlock *block) { delete block; });       \
    result->inputTypes = static_cast<CBInputTypesProc>(                        \
        [](CBlock *block) { return CBTypesInfo(); });                          \
    result->outputTypes = static_cast<CBOutputTypesProc>(                      \
        [](CBlock *block) { return CBTypesInfo(); });                          \
    result->exposedVariables = static_cast<CBExposedVariablesProc>(            \
        [](CBlock *block) { return CBExposedTypesInfo(); });                   \
    result->consumedVariables = static_cast<CBConsumedVariablesProc>(          \
        [](CBlock *block) { return CBExposedTypesInfo(); });                   \
    result->parameters = static_cast<CBParametersProc>(                        \
        [](CBlock *block) { return CBParametersInfo(); });                     \
    result->setParam = static_cast<CBSetParamProc>(                            \
        [](CBlock *block, int index, CBVar value) {});                         \
    result->getParam = static_cast<CBGetParamProc>(                            \
        [](CBlock *block, int index) { return CBVar(); });                     \
    result->inferTypes = nullptr;                                              \
    result->cleanup = static_cast<CBCleanupProc>([](CBlock *block) {});

#define RUNTIME_BLOCK_TYPE(_namespace_, _name_)                                \
  struct _name_##Runtime {                                                     \
    CBlock header;                                                             \
    _name_ core;                                                               \
  };
#define RUNTIME_BLOCK_FACTORY(_namespace_, _name_)                             \
  __cdecl CBlock *createBlock##_name_() {                                      \
    CBlock *result = reinterpret_cast<CBlock *>(new _name_##Runtime());        \
    result->name = static_cast<CBNameProc>(                                    \
        [](CBlock *block) { return #_namespace_ "." #_name_; });               \
    result->help = static_cast<CBHelpProc>([](CBlock *block) { return ""; });  \
    result->setup = static_cast<CBSetupProc>([](CBlock *block) {});            \
    result->destroy =                                                          \
        static_cast<CBDestroyProc>([](CBlock *block) { delete block; });       \
    result->inputTypes = static_cast<CBInputTypesProc>(                        \
        [](CBlock *block) { return CBTypesInfo(); });                          \
    result->outputTypes = static_cast<CBOutputTypesProc>(                      \
        [](CBlock *block) { return CBTypesInfo(); });                          \
    result->exposedVariables = static_cast<CBExposedVariablesProc>(            \
        [](CBlock *block) { return CBExposedTypesInfo(); });                   \
    result->consumedVariables = static_cast<CBConsumedVariablesProc>(          \
        [](CBlock *block) { return CBExposedTypesInfo(); });                   \
    result->parameters = static_cast<CBParametersProc>(                        \
        [](CBlock *block) { return CBParametersInfo(); });                     \
    result->setParam = static_cast<CBSetParamProc>(                            \
        [](CBlock *block, int index, CBVar value) {});                         \
    result->getParam = static_cast<CBGetParamProc>(                            \
        [](CBlock *block, int index) { return CBVar(); });                     \
    result->inferTypes = nullptr;                                              \
    result->cleanup = static_cast<CBCleanupProc>([](CBlock *block) {});

#define RUNTIME_CORE_BLOCK_TYPE(_name_)                                        \
  struct _name_##Runtime {                                                     \
    CBlock header;                                                             \
    _name_ core;                                                               \
  };
#define RUNTIME_CORE_BLOCK_FACTORY(_name_)                                     \
  __cdecl CBlock *createBlock##_name_() {                                      \
    CBlock *result = reinterpret_cast<CBlock *>(new _name_##Runtime());        \
    result->name =                                                             \
        static_cast<CBNameProc>([](CBlock *block) { return #_name_; });        \
    result->help = static_cast<CBHelpProc>([](CBlock *block) { return ""; });  \
    result->setup = static_cast<CBSetupProc>([](CBlock *block) {});            \
    result->destroy =                                                          \
        static_cast<CBDestroyProc>([](CBlock *block) { delete block; });       \
    result->inputTypes = static_cast<CBInputTypesProc>(                        \
        [](CBlock *block) { return CBTypesInfo(); });                          \
    result->outputTypes = static_cast<CBOutputTypesProc>(                      \
        [](CBlock *block) { return CBTypesInfo(); });                          \
    result->exposedVariables = static_cast<CBExposedVariablesProc>(            \
        [](CBlock *block) { return CBExposedTypesInfo(); });                   \
    result->consumedVariables = static_cast<CBConsumedVariablesProc>(          \
        [](CBlock *block) { return CBExposedTypesInfo(); });                   \
    result->parameters = static_cast<CBParametersProc>(                        \
        [](CBlock *block) { return CBParametersInfo(); });                     \
    result->setParam = static_cast<CBSetParamProc>(                            \
        [](CBlock *block, int index, CBVar value) {});                         \
    result->getParam = static_cast<CBGetParamProc>(                            \
        [](CBlock *block, int index) { return CBVar(); });                     \
    result->inferTypes = nullptr;                                              \
    result->cleanup = static_cast<CBCleanupProc>([](CBlock *block) {});

// Those get nicely inlined fully so only 1 indirection will happen at the root
// of the call if the block is all inline
#define RUNTIME_BLOCK_name(_name_)                                             \
  result->name = static_cast<CBSetupProc>([](CBlock *block) {                  \
    return reinterpret_cast<_name_##Runtime *>(block)->core.name();            \
  });
#define RUNTIME_BLOCK_help(_name_)                                             \
  result->help = static_cast<CBHelpProc>([](CBlock *block) {                   \
    return reinterpret_cast<_name_##Runtime *>(block)->core.help();            \
  });

#define RUNTIME_BLOCK_setup(_name_)                                            \
  result->setup = static_cast<CBSetupProc>([](CBlock *block) {                 \
    reinterpret_cast<_name_##Runtime *>(block)->core.setup();                  \
  });
#define RUNTIME_BLOCK_destroy(_name_)                                          \
  result->destroy = static_cast<CBDestroyProc>([](CBlock *block) {             \
    reinterpret_cast<_name_##Runtime *>(block)->core.destroy();                \
    delete block;                                                              \
  });

#define RUNTIME_BLOCK_inputTypes(_name_)                                       \
  result->inputTypes = static_cast<CBInputTypesProc>([](CBlock *block) {       \
    return reinterpret_cast<_name_##Runtime *>(block)->core.inputTypes();      \
  });
#define RUNTIME_BLOCK_outputTypes(_name_)                                      \
  result->outputTypes = static_cast<CBOutputTypesProc>([](CBlock *block) {     \
    return reinterpret_cast<_name_##Runtime *>(block)->core.outputTypes();     \
  });

#define RUNTIME_BLOCK_exposedVariables(_name_)                                 \
  result->exposedVariables =                                                   \
      static_cast<CBExposedVariablesProc>([](CBlock *block) {                  \
        return reinterpret_cast<_name_##Runtime *>(block)                      \
            ->core.exposedVariables();                                         \
      });
#define RUNTIME_BLOCK_consumedVariables(_name_)                                \
  result->consumedVariables =                                                  \
      static_cast<CBConsumedVariablesProc>([](CBlock *block) {                 \
        return reinterpret_cast<_name_##Runtime *>(block)                      \
            ->core.consumedVariables();                                        \
      });

#define RUNTIME_BLOCK_parameters(_name_)                                       \
  result->parameters = static_cast<CBParametersProc>([](CBlock *block) {       \
    return reinterpret_cast<_name_##Runtime *>(block)->core.parameters();      \
  });
#define RUNTIME_BLOCK_setParam(_name_)                                         \
  result->setParam = static_cast<CBSetParamProc>([](CBlock *block, int index,  \
                                                    CBVar value) {             \
    reinterpret_cast<_name_##Runtime *>(block)->core.setParam(index, value);   \
  });
#define RUNTIME_BLOCK_getParam(_name_)                                         \
  result->getParam = static_cast<CBGetParamProc>([](CBlock *block,             \
                                                    int index) {               \
    return reinterpret_cast<_name_##Runtime *>(block)->core.getParam(index);   \
  });

#define RUNTIME_BLOCK_inferTypes(_name_)                                       \
  result->inferTypes = static_cast<CBInferTypesProc>(                          \
      [](CBlock *block, CBTypeInfo inputType,                                  \
         CBExposedTypesInfo consumableVariables) {                             \
        return reinterpret_cast<_name_##Runtime *>(block)->core.inferTypes(    \
            inputType, consumableVariables);                                   \
      });

#define RUNTIME_BLOCK_activate(_name_)                                         \
  result->activate = static_cast<CBActivateProc>(                              \
      [](CBlock *block, CBContext *context, const CBVar *input) {              \
        return reinterpret_cast<_name_##Runtime *>(block)->core.activate(      \
            context, *input);                                                  \
      });

#define RUNTIME_BLOCK_cleanup(_name_)                                          \
  result->cleanup = static_cast<CBCleanupProc>([](CBlock *block) {             \
    reinterpret_cast<_name_##Runtime *>(block)->core.cleanup();                \
  });

#define RUNTIME_BLOCK_END(_name_)                                              \
  return result;                                                               \
  }

#define REGISTER_BLOCK(_namespace_, _name_)                                    \
  chainblocks::registerBlock(#_namespace_ "." #_name_,                         \
                             _namespace_::createBlock##_name_)
#define REGISTER_CORE_BLOCK(_name_)                                            \
  chainblocks::registerBlock(#_name_, createBlock##_name_)
