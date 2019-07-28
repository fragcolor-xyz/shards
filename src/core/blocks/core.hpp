#include "../chainblocks.hpp"
#include "../blocks_macros.hpp"

namespace chainblocks
{
  struct Msg
  {
    CBVar msg;
    
    void destroy()
    {
      destroyVar(msg);
    }
    
    CBTypesInfo inputTypes()
    {
      static CBTypeInfo info[] = {{ Any, true /*sequenced*/ }};
      return info;
    }
    
    CBTypesInfo outputTypes()
    {
      static CBTypeInfo info[] = {{ Any, true /*sequenced*/ }};
      return info;
    }
    
    CBParametersInfo parameters()
    {
      static CBTypeInfo strInfo[] = {{ String, false /*sequenced*/ }};
      static CBParameterInfo info[] = {{ "Message", "The message to output.", strInfo}};
      return info;
    }
    
    void setParam(int index, CBVar value)
    {
      destroyVar(msg);
      cloneVar(msg, value);
    }
    
    CBVar getParam(int index)
    {
      return msg;
    }
    
    CBVar activate(CBContext* context, CBVar input)
    {
      //TODO
      return msg;
    }
  };

  struct SetTableValue
  {
    
  };
};

// Register Msg
RUNTIME_BLOCK(chainblocks::Msg, Core, Msg)
RUNTIME_BLOCK_destroy(Msg)
RUNTIME_BLOCK_inputTypes(Msg)
RUNTIME_BLOCK_outputTypes(Msg)
RUNTIME_BLOCK_parameters(Msg)
RUNTIME_BLOCK_setParam(Msg)
RUNTIME_BLOCK_getParam(Msg)
RUNTIME_BLOCK_activate(Msg)
RUNTIME_BLOCK_END(Msg)

static void registerCoreBlocks()
{
  REGISTER_BLOCK(Core, Msg);
}