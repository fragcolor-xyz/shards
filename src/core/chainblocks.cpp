#define DG_DYNARR_IMPLEMENTATION 1
#define GB_STRING_IMPLEMENTATION 1
#define CHAINBLOCKS_RUNTIME 1
#define DLL_EXPORT 1
#include "chainblocks.hpp"

namespace chainblocks
{
  std::unordered_map<std::string, CBBlockConstructor> BlocksRegister;
  std::unordered_map<std::tuple<int32_t, int32_t>, CBObjectInfo> ObjectTypesRegister;
  std::unordered_map<std::string, CBVar> GlobalVariables;
  std::unordered_map<std::string, CBOnRunLoopTick> RunLoopHooks;
  std::unordered_map<std::string, CBChain**> GlobalChains;
  thread_local CBChain* CurrentChain;
};

#ifdef __cplusplus
extern "C" {
#endif

EXPORTED void __cdecl chainblocks_RegisterBlock(const char* fullName, CBBlockConstructor constructor)
{
  chainblocks::registerBlock(fullName, constructor);
}

EXPORTED void __cdecl chainblocks_RegisterObjectType(int32_t vendorId, int32_t typeId, CBObjectInfo info)
{
  chainblocks::registerObjectType(vendorId, typeId, info);
}

EXPORTED void __cdecl chainblocks_RegisterRunLoopCallback(const char* eventName, CBOnRunLoopTick callback)
{
  chainblocks::registerRunLoopCallback(eventName, callback);
}

EXPORTED void __cdecl chainblocks_UnregisterRunLoopCallback(const char* eventName)
{
  chainblocks::unregisterRunLoopCallback(eventName);
}

EXPORTED CBVar* __cdecl chainblocks_ContextVariable(CBContext* context, const char* name)
{
  return chainblocks::contextVariable(context, name);
}

EXPORTED void __cdecl chainblocks_SetError(CBContext* context, const char* errorText)
{
  context->setError(errorText);
}

EXPORTED CBVar __cdecl chainblocks_Suspend(double seconds)
{
  return chainblocks::suspend(seconds);
}

#ifdef __cplusplus
};
#endif

#ifdef TESTING
  static CBChain mainChain("MainChain");

  int main()
  {
    {
      CBRuntimeBlock blk = {};
    }
    
    {
      auto ublk = std::make_unique<CBRuntimeBlock>();
    }
    
    chainblocks::CurrentChain = &mainChain;
    chainblocks::start(chainblocks::CurrentChain, true);
    chainblocks::tick(chainblocks::CurrentChain);
    chainblocks::tick(chainblocks::CurrentChain);
    chainblocks::stop(chainblocks::CurrentChain);

    chainblocks::GlobalVariables.insert(std::make_pair("TestVar", CBVar(Restart)));
    auto tv = chainblocks::globalVariable(u8"TestVar");
    if(tv->valueType == None && tv->chainState == Restart)
    {
      printf("Map ok!\n");
    }

    auto tv2 = chainblocks::globalVariable("TestVar2");
    if(tv2->valueType == None && tv2->chainState == Continue)
    {
      printf("Map ok2!\n");
    }

    printf("Size: %d\n", sizeof(CBVar));
  }
#endif