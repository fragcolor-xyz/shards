#define RUNTIME_BLOCK(_typename_, _namespace_, _name_)namespace _namespace_ {\
  struct _name_##Runtime {\
    CBRuntimeBlock header;\
    _typename_ core;\
  };\
  __cdecl CBRuntimeBlock* createBlock##_name_() {\
    CBRuntimeBlock* result = reinterpret_cast<CBRuntimeBlock*>(new _name_##Runtime());

#define RUNTIME_BLOCK_name(_x_, _y_) "\""#_x_"\""
#define RUNTIME_BLOCK_help(_x_, _y_)

// Those get nicely inlined fully so only 1 indirection will happen at the root of the call if the block is all inline
#define RUNTIME_BLOCK_setup(_name_)    result->setup = static_cast<CBSetupProc>([] (CBRuntimeBlock* block) { reinterpret_cast<_name_##Runtime*>(block)->core.setup(); });
#define RUNTIME_BLOCK_destroy(_name_)    result->destroy = static_cast<CBDestroyProc>([] (CBRuntimeBlock* block) { reinterpret_cast<_name_##Runtime*>(block)->core.destroy(); });
#define RUNTIME_BLOCK_inputTypes(_name_)    result->inputTypes = static_cast<CBInputTypesProc>([] (CBRuntimeBlock* block) { return reinterpret_cast<_name_##Runtime*>(block)->core.inputTypes(); });
#define RUNTIME_BLOCK_outputTypes(_name_)    result->outputTypes = static_cast<CBOutputTypesProc>([] (CBRuntimeBlock* block) { return reinterpret_cast<_name_##Runtime*>(block)->core.outputTypes(); });
#define RUNTIME_BLOCK_parameters(_name_)    result->parameters = static_cast<CBParametersProc>([] (CBRuntimeBlock* block) { return reinterpret_cast<_name_##Runtime*>(block)->core.parameters(); });
#define RUNTIME_BLOCK_setParam(_name_)    result->setParam = static_cast<CBSetParamProc>([] (CBRuntimeBlock* block, int index, CBVar value) { reinterpret_cast<_name_##Runtime*>(block)->core.setParam(index, value); });
#define RUNTIME_BLOCK_getParam(_name_)    result->getParam = static_cast<CBGetParamProc>([] (CBRuntimeBlock* block, int index) { return reinterpret_cast<_name_##Runtime*>(block)->core.getParam(index); });
#define RUNTIME_BLOCK_activate(_name_)    result->activate = static_cast<CBActivateProc>([] (CBRuntimeBlock* block, CBContext* context, CBVar input) { return reinterpret_cast<_name_##Runtime*>(block)->core.activate(context, input); });

// #define RUNTIME_BLOCK_preChain(_x_, _y_)
// #define RUNTIME_BLOCK_postChain(_x_, _y_)
// #define RUNTIME_BLOCK_inputTypes(_x_, _y_)
// #define RUNTIME_BLOCK_outputTypes(_x_, _y_)
// #define RUNTIME_BLOCK_exposedVariables(_x_, _y_)
// #define RUNTIME_BLOCK_consumedVariables(_x_, _y_)
// #define RUNTIME_BLOCK_parameters(_x_, _y_)
// #define RUNTIME_BLOCK_setParam(_x_, _y_)
// #define RUNTIME_BLOCK_getParam(_x_, _y_)
// #define RUNTIME_BLOCK_activate(_x_, _y_)
// #define RUNTIME_BLOCK_cleanup(_x_, _y_)

#define RUNTIME_BLOCK_END()  return result;\
  }\
}; //namespace ending

