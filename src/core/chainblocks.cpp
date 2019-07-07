#define DG_DYNARR_IMPLEMENTATION 1
#define GB_STRING_IMPLEMENTATION 1
#define CHAINBLOCKS_RUNTIME 1
#define DLL_EXPORT 1
#include "chainblocks.hpp"

#ifdef __cplusplus
extern "C" {
#endif

EXPORTED void __cdecl chainblocks_RegisterBlock(CBRegistry* registry, CBString fullName, CBBlockConstructor constructor)
{
  chainblocks::registerBlock(*registry, fullName, constructor);
}

EXPORTED void __cdecl chainblocks_RegisterObjectType(CBRegistry* registry, int32_t vendorId, int32_t typeId, CBObjectInfo info)
{
  chainblocks::registerObjectType(*registry, vendorId, typeId, info);
}

EXPORTED CBVar* __cdecl chainblocks_ContextVariable(CBContext* context, CBString name)
{
  return chainblocks::contextVariable(context, name);
}

#ifdef __cplusplus
};
#endif

#ifdef TESTING
  static CBChain mainChain;

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
    auto tv = chainblocks::globalVariable("TestVar");
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