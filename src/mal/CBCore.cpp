#define String MalString
#include "MAL.h"
#include "Environment.h"
#include "StaticList.h"
#include "Types.h"
#undef String
#include <algorithm>
#include <set>
#define CHAINBLOCKS_RUNTIME
#include "../core/runtime.hpp"

#define CHECK_ARGS_IS(expected) \
    checkArgsIs(name.c_str(), expected, \
                  std::distance(argsBegin, argsEnd))

#define CHECK_ARGS_BETWEEN(min, max) \
    checkArgsBetween(name.c_str(), min, max, \
                       std::distance(argsBegin, argsEnd))

#define CHECK_ARGS_AT_LEAST(expected) \
    checkArgsAtLeast(name.c_str(), expected, \
                        std::distance(argsBegin, argsEnd))

#define ARG(type, name) type* name = VALUE_CAST(type, *argsBegin++)

static StaticList<malBuiltIn*> handlers;
#define FUNCNAME(uniq) builtIn ## uniq
#define HRECNAME(uniq) handler ## uniq
#define BUILTIN_DEF(uniq, symbol) \
    static malBuiltIn::ApplyFunc FUNCNAME(uniq); \
    static StaticList<malBuiltIn*>::Node HRECNAME(uniq) \
        (handlers, new malBuiltIn(symbol, FUNCNAME(uniq))); \
    malValuePtr FUNCNAME(uniq)(const MalString& name, \
        malValueIter argsBegin, malValueIter argsEnd)

#define BUILTIN(symbol)  BUILTIN_DEF(__LINE__, symbol)

#define BUILTIN_ISA(symbol, type) \
    BUILTIN(symbol) { \
        CHECK_ARGS_IS(1); \
        return mal::boolean(DYNAMIC_CAST(type, *argsBegin)); \
    }

extern void NimMain();

void registerKeywords(malEnvPtr env);

void installCBCore(malEnvPtr env) 
{
  NimMain();
  
  registerKeywords(env);
  
  env->set("Looped", mal::keyword("Looped"));
  env->set("Unsafe", mal::keyword("Unsafe"));
  
  for (auto it = handlers.begin(), end = handlers.end(); it != end; ++it) 
  {
    malBuiltIn* handler = *it;
    env->set(handler->name(), handler);
  }
}

class malCBChain : public malValue {
public:
    malCBChain(MalString name) : m_chain(new CBChain(name.c_str())) { }
    
    malCBChain(const malCBChain& that, malValuePtr meta) : malValue(meta), m_chain(that.m_chain) { }
    
    ~malCBChain()
    {
      delete m_chain;
    }
    
    virtual MalString print(bool readably) const 
    { 
      std::ostringstream stream;
      stream << "CBChain: " << m_chain->name;
      return stream.str();
    }
    
    CBChain* value() const { return m_chain; }
    
    virtual bool doIsEqualTo(const malValue* rhs) const
    {
      return m_chain == static_cast<const malCBChain*>(rhs)->m_chain;
    }
    
    WITH_META(malCBChain);
private:
    CBChain* m_chain;
};

class malCBBlock : public malValue {
public:
    malCBBlock(MalString name) : m_block(chainblocks::createBlock(name.c_str())) { }
    
    malCBBlock(CBRuntimeBlock* block) : m_block(block) { }
    
    malCBBlock(const malCBBlock& that, malValuePtr meta) : malValue(meta), m_block(that.m_block) { }
    
    ~malCBBlock()
    {
      if(m_block)
        m_block->destroy(m_block);
    }
    
    virtual MalString print(bool readably) const 
    { 
      std::ostringstream stream;
      stream << "Block: " << m_block->name(m_block);
      return stream.str();
    }
    
    CBRuntimeBlock* value() const { return m_block; }
    
    void consume() 
    {
      m_block = nullptr;
    }
    
    void addChain(const malCBChain* chain)
    {
      m_innerRefs.insert(chain->acquire());
    }
    
    virtual bool doIsEqualTo(const malValue* rhs) const
    {
      return m_block == static_cast<const malCBBlock*>(rhs)->m_block;
    }
    
    WITH_META(malCBBlock);
private:
    CBRuntimeBlock* m_block;
    std::set<const RefCounted*> m_innerRefs;
};

class malCBNode : public malValue {
public:
    malCBNode() : m_node(new CBNode()) { }
    
    malCBNode(const malCBNode& that, malValuePtr meta) : malValue(meta), m_node(that.m_node) { }
    
    ~malCBNode()
    {
      delete m_node;
    }
    
    virtual MalString print(bool readably) const 
    { 
      std::ostringstream stream;
      stream << "CBNode";
      return stream.str();
    }

    CBNode* value() const { return m_node; }

    void schedule(malCBChain* chain)
    {
      m_node->schedule(chain->value());
      m_innerRefs.insert(chain->acquire());
    }
    
    virtual bool doIsEqualTo(const malValue* rhs) const
    {
      return m_node == static_cast<const malCBNode*>(rhs)->m_node;
    }
    
    WITH_META(malCBNode);
private:
    CBNode* m_node;
    std::set<const RefCounted*> m_innerRefs;
};

class malCBVar : public malValue {
public:
    malCBVar(CBVar var) : m_var(var) { }
    
    malCBVar(const malCBVar& that, malValuePtr meta) : malValue(meta) 
    {
      chainblocks::destroyVar(m_var);
      chainblocks::cloneVar(m_var, that.m_var);
    }

    ~malCBVar()
    {
      chainblocks::destroyVar(m_var);
    }
    
    virtual MalString print(bool readably) const 
    { 
      std::ostringstream stream;
      stream << "CBVar: " << m_var;
      return stream.str();
    }
    
    virtual bool doIsEqualTo(const malValue* rhs) const
    {
      return m_var == static_cast<const malCBVar*>(rhs)->m_var;
    }

    CBVar value() const { return m_var; }
    
    WITH_META(malCBVar);
private:
    CBVar m_var;
};

BUILTIN("Node")
{
  return malValuePtr(new malCBNode());
}

#define WRAP_TO_CONST(_var_) auto constBlock = chainblocks::createBlock("Const");\
  constBlock->setup(constBlock);\
  constBlock->setParam(constBlock, 0, _var_);\
  return constBlock

// Helper to generate const blocks automatically inferring types
CBRuntimeBlock* blockify(malValuePtr arg)
{
  if (arg == mal::nilValue())
  {
    // Wrap none into const
    CBVar var;
    var.valueType = None;
    var.payload.chainState = Continue;
    WRAP_TO_CONST(var);
  }
  else if (const malString* v = DYNAMIC_CAST(malString, arg)) 
  {
    CBVar strVar;
    strVar.valueType = String;
    auto s = v->value();
    strVar.payload.stringValue = s.c_str();
    WRAP_TO_CONST(strVar);
  }
  else if (const malNumber* v = DYNAMIC_CAST(malNumber, arg)) 
  {
    auto value = v->value();
    int64_t floatTest = value * 10;
    bool isInteger = (floatTest % 10) == 0;
    CBVar var;
    if(isInteger)
    {
      var.valueType = Int;
      var.payload.intValue = value;
    }
    else
    {
      var.valueType = Float;
      var.payload.floatValue = value;
    }
    WRAP_TO_CONST(var);
  }
  else if (arg == mal::trueValue())
  {
    CBVar var;
    var.valueType = Bool;
    var.payload.boolValue = true;
    WRAP_TO_CONST(var);
  }
  else if (arg == mal::falseValue())
  {
    CBVar var;
    var.valueType = Bool;
    var.payload.boolValue = false;
    WRAP_TO_CONST(var);
  }
  else if (const malCBVar* v = DYNAMIC_CAST(malCBVar, arg)) 
  {
    WRAP_TO_CONST(v->value());
  }
  else if (const malCBBlock* v = DYNAMIC_CAST(malCBBlock, arg)) 
  {
    auto block = v->value();
    const_cast<malCBBlock*>(v)->consume(); // Blocks are unique, consume before use, assume goes inside another block or
    return block;
  }
  else
  {
    throw chainblocks::CBException("Invalid argument");
  }
}

CBVar varify(malCBBlock* mblk, malValuePtr arg)
{
  // Returns clones in order to proper cleanup (nested) allocations
  if (arg == mal::nilValue())
  {
    CBVar var;
    var.valueType = None;
    var.payload.chainState = Continue;
    return var;
  }
  else if (const malString* v = DYNAMIC_CAST(malString, arg)) 
  {
    CBVar tmp;
    tmp.valueType = String;
    auto s = v->value();
    tmp.payload.stringValue = s.c_str();
    auto var = CBVar();
    chainblocks::cloneVar(var, tmp);
    return var;
  }
  else if (const malNumber* v = DYNAMIC_CAST(malNumber, arg)) 
  {
    auto value = v->value();
    int64_t floatTest = value * 10;
    bool isInteger = (floatTest % 10) == 0;
    CBVar var;
    if(isInteger)
    {
      var.valueType = Int;
      var.payload.intValue = value;
    }
    else
    {
      var.valueType = Float;
      var.payload.floatValue = value;
    }
    return var;
  }
  else if (const malList* v = DYNAMIC_CAST(malList, arg)) 
  {
    CBVar tmp;
    tmp.valueType = Seq;
    tmp.payload.seqValue = nullptr;
    tmp.payload.seqLen = -1;
    auto count = v->count();
    for(auto i = 0; i < count; i++)
    {
      auto val = v->item(i);
      auto subVar = varify(mblk, val);
      stbds_arrpush(tmp.payload.seqValue, subVar);
    }
    auto var = CBVar();
    chainblocks::cloneVar(var, tmp);
    stbds_arrfree(tmp.payload.seqValue);
    return var;
  }
  else if (arg == mal::trueValue())
  {
    CBVar var;
    var.valueType = Bool;
    var.payload.boolValue = true;
    return var;
  }
  else if (arg == mal::falseValue())
  {
    CBVar var;
    var.valueType = Bool;
    var.payload.boolValue = false;
    return var;
  }
  else if (const malCBVar* v = DYNAMIC_CAST(malCBVar, arg)) 
  {
    auto var = CBVar();
    chainblocks::cloneVar(var, v->value());
    return var;
  }
  else if (const malCBBlock* v = DYNAMIC_CAST(malCBBlock, arg)) 
  {
    auto block = v->value();
    const_cast<malCBBlock*>(v)->consume(); // Blocks are unique, consume before use, assume goes inside another block or
    CBVar var;
    var.valueType = Block;
    var.payload.blockValue = block;
    return var;
  }
  else if (const malCBChain* v = DYNAMIC_CAST(malCBChain, arg)) 
  {
    auto chain = v->value();
    CBVar var;
    var.valueType = Chain;
    var.payload.chainValue = chain;
    mblk->addChain(v);
    return var;
  }
  else
  {
    throw chainblocks::CBException("Invalid variable");
  }
}

int findParamIndex(std::string name, CBParametersInfo params)
{
  for(auto i = 0; i < stbds_arrlen(params); i++)
  {
    auto param = params[i];
    if(param.name == name)
      return i;
  }
  return -1;
}

void validationCallback(const CBRuntimeBlock* errorBlock, const char* errorTxt, bool nonfatalWarning, void* userData)
{
  auto failed = reinterpret_cast<bool*>(userData);
  auto block = const_cast<CBRuntimeBlock*>(errorBlock);
  if(!nonfatalWarning)
  {
    *failed = true;
    LOG(ERROR) << "Parameter validation failed: " << errorTxt << " block: " << block->name(block);
  }
  else
  {
    LOG(INFO) << "Parameter validation warning: " << errorTxt << " block: " << block->name(block);
  }
}

void setBlockParameters(malCBBlock* malblock, malValueIter begin, malValueIter end)
{
  auto block = malblock->value();
  auto argsBegin = begin;
  auto argsEnd = end;
  auto params = block->parameters(block);
  auto pindex = 0;
  while(argsBegin != argsEnd)
  {
    auto arg = *argsBegin++;
    if(const malKeyword* v = DYNAMIC_CAST(malKeyword, arg))
    {
      // In this case it's a keyworded call from now on
      pindex = -1;
      
      // Jump to next value straight, expecting a value
      auto value = *argsBegin++;
      auto paramName = v->value().substr(1);
      auto idx = findParamIndex(paramName, params);
      if(unlikely(idx == -1))
      {
        LOG(ERROR) << "Parameter not found: " << paramName << " block: " << block->name(block);
        throw chainblocks::CBException("Parameter not found");
      }
      else
      {
        auto var = varify(malblock, value);
        bool failed = false;
        validateSetParam(block, idx, var, validationCallback, &failed);
        if(failed)
          throw chainblocks::CBException("Parameter validation failed");
        block->setParam(block, idx, var);
        chainblocks::destroyVar(var);
      }
    }
    else if(pindex == -1)
    {
      // We expected a keyword! fail
      LOG(ERROR) << "Keyword expected, block: " << block->name(block);
      throw chainblocks::CBException("Keyword expected");
    }
    else
    {
      auto var = varify(malblock, arg);
      bool failed = false;
      validateSetParam(block, pindex, var, validationCallback, &failed);
      if(failed)
        throw chainblocks::CBException("Parameter validation failed");
      block->setParam(block, pindex, var);
      chainblocks::destroyVar(var);
      
      pindex++;
    }
  }
}

BUILTIN("Chain")
{
  CHECK_ARGS_AT_LEAST(1);
  ARG(malString, chainName);
  auto mchain = new malCBChain(chainName->value());
  auto chain = mchain->value();
  while(argsBegin != argsEnd)
  {
    auto arg = *argsBegin++;
    if (const malKeyword* v = DYNAMIC_CAST(malKeyword, arg))  // Options!
    {
      if(v->value() == ":Looped")
      {
        chain->looped = true;
      }
      else if(v->value() == ":Unsafe")
      {
        chain->unsafe = true;
      }
    }
    else
    {
      chain->addBlock(blockify(arg));
    }
  }
  return malValuePtr(mchain);
}

BUILTIN("Blocks")
{
  auto vec = new malValueVec();
  while(argsBegin != argsEnd)
  {
    auto arg = *argsBegin++;
    vec->push_back(malValuePtr(new malCBBlock(blockify(arg))));
  }
  return malValuePtr(new malList(vec));
}

BUILTIN("schedule")
{
  CHECK_ARGS_IS(2);
  ARG(malCBNode, node);
  ARG(malCBChain, chain);
  node->schedule(chain);
  return mal::nilValue();
}

BUILTIN("tick")
{
  CHECK_ARGS_IS(1);
  ARG(malCBNode, node);
  auto noErrors = node->value()->tick();
  return mal::boolean(noErrors);
}

BUILTIN("sleep")
{
  CHECK_ARGS_IS(1);
  ARG(malNumber, sleepTime);
  chainblocks::sleep(sleepTime->value());
  return mal::nilValue();
}

BUILTIN("#")
{
  CHECK_ARGS_IS(1);
  ARG(malString, value);
  CBVar tmp;
  tmp.valueType = ContextVar;
  auto s = value->value();
  tmp.payload.stringValue = s.c_str();
  auto var = CBVar();
  chainblocks::cloneVar(var, tmp);
  return malValuePtr(new malCBVar(var));
}

BUILTIN("Enum")
{
  CHECK_ARGS_IS(3);
  ARG(malNumber, value0);
  ARG(malNumber, value1);
  ARG(malNumber, value2);
  CBVar var;
  var.valueType = Enum;
  var.payload.enumVendorId = static_cast<int32_t>(value0->value());
  var.payload.enumTypeId = static_cast<int32_t>(value1->value());
  var.payload.enumValue = static_cast<CBEnum>(value2->value());
  return malValuePtr(new malCBVar(var));
}

BUILTIN("Int")
{
  CHECK_ARGS_IS(1);
  ARG(malNumber, value);
  CBVar var;
  var.valueType = Int;
  var.payload.intValue = value->value();
  return malValuePtr(new malCBVar(var));
}

BUILTIN("Int2")
{
  CHECK_ARGS_IS(2);
  ARG(malNumber, value0);
  ARG(malNumber, value1);
  CBVar var;
  var.valueType = Int2;
  var.payload.int2Value[0] = value0->value();
  var.payload.int2Value[1] = value1->value();
  return malValuePtr(new malCBVar(var));
}

BUILTIN("Int3")
{
  CHECK_ARGS_IS(3);
  ARG(malNumber, value0);
  ARG(malNumber, value1);
  ARG(malNumber, value2);
  CBVar var;
  var.valueType = Int3;
  var.payload.int3Value[0] = value0->value();
  var.payload.int3Value[1] = value1->value();
  var.payload.int3Value[2] = value2->value();
  return malValuePtr(new malCBVar(var));
}

BUILTIN("Int4")
{
  CHECK_ARGS_IS(4);
  ARG(malNumber, value0);
  ARG(malNumber, value1);
  ARG(malNumber, value2);
  ARG(malNumber, value3);
  CBVar var;
  var.valueType = Int4;
  var.payload.int4Value[0] = value0->value();
  var.payload.int4Value[1] = value1->value();
  var.payload.int4Value[2] = value2->value();
  var.payload.int4Value[3] = value3->value();
  return malValuePtr(new malCBVar(var));
}

BUILTIN("Color")
{
  CHECK_ARGS_IS(4);
  ARG(malNumber, value0);
  ARG(malNumber, value1);
  ARG(malNumber, value2);
  ARG(malNumber, value3);
  CBVar var;
  var.valueType = Color;
  var.payload.colorValue.r = static_cast<uint8_t>(value0->value());
  var.payload.colorValue.g = static_cast<uint8_t>(value1->value());
  var.payload.colorValue.b = static_cast<uint8_t>(value2->value());
  var.payload.colorValue.a = static_cast<uint8_t>(value3->value());
  return malValuePtr(new malCBVar(var));
}

BUILTIN("Float")
{
  CHECK_ARGS_IS(1);
  ARG(malNumber, value);
  CBVar var;
  var.valueType = Float;
  var.payload.floatValue = value->value();
  return malValuePtr(new malCBVar(var));
}

BUILTIN("Float2")
{
  CHECK_ARGS_IS(2);
  ARG(malNumber, value0);
  ARG(malNumber, value1);
  CBVar var;
  var.valueType = Float2;
  var.payload.float2Value[0] = value0->value();
  var.payload.float2Value[1] = value1->value();
  return malValuePtr(new malCBVar(var));
}

BUILTIN("Float3")
{
  CHECK_ARGS_IS(3);
  ARG(malNumber, value0);
  ARG(malNumber, value1);
  ARG(malNumber, value2);
  CBVar var;
  var.valueType = Float3;
  var.payload.float3Value[0] = value0->value();
  var.payload.float3Value[1] = value1->value();
  var.payload.float3Value[2] = value2->value();
  return malValuePtr(new malCBVar(var));
}

BUILTIN("Float4")
{
  CHECK_ARGS_IS(4);
  ARG(malNumber, value0);
  ARG(malNumber, value1);
  ARG(malNumber, value2);
  ARG(malNumber, value3);
  CBVar var;
  var.valueType = Float4;
  var.payload.float4Value[0] = value0->value();
  var.payload.float4Value[1] = value1->value();
  var.payload.float4Value[2] = value2->value();
  var.payload.float4Value[3] = value3->value();
  return malValuePtr(new malCBVar(var));
}

BUILTIN_ISA("Var?", malCBVar);
BUILTIN_ISA("Node?", malCBNode);
BUILTIN_ISA("Chain?", malCBChain);
BUILTIN_ISA("Block?", malCBBlock);

#ifdef HAS_CB_GENERATED
#include "CBGenerated.hpp"
#endif