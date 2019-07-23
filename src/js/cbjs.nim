import quicknimjs
import ../nim/chainblocks

var
  jsRuntime = newJSRuntime()
  jsContext = newJSContext(jsRuntime)

proc loadBuiltIns() =
  var echoProc = newProc(jsContext, proc(ctx: ptr JSContext; this_val: JSValue; argc: cint; argv: ptr UncheckedArray[JSValue]): JSValue {.cdecl.} =
    for i in 0..<argc:
      echo argv[i].toStr(jsContext).getStr(jsContext)
  , "echo", 1)
  jsContext.Global.setProperty(jsContext, "echo", echoProc)
loadBuiltIns()

const
  JsVarProp_StorageType = 1

type JsVarStorage {.pure.} = enum
  None,
  Copy

var JsCBVarConst = JsClass(
  finalizer: proc(self: JSValue; classId: uint32) =
    discard,
  constructor: proc(self: JSValue; context: ptr JSContext; argc: int; argv: ptr UncheckedArray[JSValue]) =
    discard,
  constructorMinArgs: 0
)
jsClass(jsContext, "ConstVar", JsCBVarConst)

JsCBVarConst.jsClassGetSet(
  "valueType",
  proc(ctx: ptr JSContext; this_val: JSValue): JSValue {.cdecl.} =
    let
      pvar = cast[ptr CBVar](this_val.getPtr(JsCBVarConst.classId))
    
    if pvar != nil:
      return pvar[].valueType.int32.jsInt32(ctx)
    else:
      return Undefined
)

proc toJSValue*(v: var CBVar; context: ptr JSContext): JSValue {.inline.} =
  result = newClassObj(context, JsCBVarConst.classId, JsCBVarConst.prototype)
  result.attachPtr(addr v)

var JsCBVar = JsClass(
  finalizer: proc(self: JSValue; classId: uint32) =
    var cbvar = cast[ptr CBVar](self.getPtr(classId))
    if cbvar != nil:
      # let storage = self.getProperty(context, ) #WIP TODO
      `~quickcopy` cbvar[]
      cppdel(cbvar),
  
  constructor: proc(self: JSValue; context: ptr JSContext; argc: int; argv: ptr UncheckedArray[JSValue]) =
    var newVar: ptr CBVar
    cppnew(newVar, CBVar, CBVar)
    
    # Add logic to init var from any value...
    if argc == 1:
      let constVar = cast[ptr CBVar](argv[0].getPtr(JsCBVarConst.classId))
      if constVar != nil:
        quickcopy(newVar[], constVar[])
        self.setProperty(context, JsVarProp_StorageType, JsVarStorage.Copy.int32.jsInt32(context))
    
    self.attachPtr(newVar),
  
  constructorMinArgs: 0
)
jsClass(jsContext, "Var", JsCBVar)

var JsCBRuntimeBlock = JsClass(
  finalizer: proc(self: JSValue; classId: uint32) =
    var blk = cast[ptr CBRuntimeBlock](self.getPtr(classId))
    if blk != nil:
      blk[].cleanup(blk)
      blk[].destroy(blk),
  
  constructor: proc(self: JSValue; context: ptr JSContext; argc: int; argv: ptr UncheckedArray[JSValue]) =
    let name = argv[0].getStr(jsContext)
    if name == nil:
      throwTypeError(context, "Block constructor requires a name")

    var blk = createBlock($name)
    self.attachPtr(blk),
  
  constructorMinArgs: 1
)
jsClass(jsContext, "Block", JsCBRuntimeBlock)

JsCBRuntimeBlock.jsClassGetSet(
  "name",
  proc(ctx: ptr JSContext; this_val: JSValue): JSValue {.cdecl.} =
    let
      blk = cast[ptr CBRuntimeBlock](this_val.getPtr(JsCBRuntimeBlock.classId))
    
    if blk != nil:
      let text = blk[].name(blk)
      return text.newStr(ctx)
    else:
      return Undefined
)
JsCBRuntimeBlock.jsClassGetSet(
  "help",
  proc(ctx: ptr JSContext; this_val: JSValue): JSValue {.cdecl.} =
    let
      blk = cast[ptr CBRuntimeBlock](this_val.getPtr(JsCBRuntimeBlock.classId))
    
    if blk != nil:
      let text = blk[].help(blk)
      return text.newStr(ctx)
    else:
      return Undefined
)
JsCBRuntimeBlock.jsClassCall(
  "setup",
  proc(ctx: ptr JSContext; this_val: JSValue; argc: cint; argv: ptr UncheckedArray[JSValue]): JSValue {.cdecl.} =
    let
      blk = cast[ptr CBRuntimeBlock](this_val.getPtr(JsCBRuntimeBlock.classId))
    
    if blk != nil:
      blk[].setup(blk)
    
    return Undefined,
)
JsCBRuntimeBlock.jsClassCall(
  "cleanup",
  proc(ctx: ptr JSContext; this_val: JSValue; argc: cint; argv: ptr UncheckedArray[JSValue]): JSValue {.cdecl.} =
    let
      blk = cast[ptr CBRuntimeBlock](this_val.getPtr(JsCBRuntimeBlock.classId))
    
    if blk != nil:
      blk[].cleanup(blk)
    
    return Undefined,
)

var JsCBChain = JsClass(
  finalizer: proc(self: JSValue; classId: uint32) =
    var chain = cast[CBChainPtr](self.getPtr(classId))
    if chain != nil:
      destroy(chain),
  
  constructor: proc(self: JSValue; context: ptr JSContext; argc: int; argv: ptr UncheckedArray[JSValue]) =
    let name = argv[0].getStr(jsContext)
    if name == nil:
      throwTypeError(context, "Chain constructor requires a name")
    
    var chain = newChain($name)
    self.attachPtr(chain),
  
  constructorMinArgs: 1
)
jsClass(jsContext, "Chain", JsCBChain)

JsCBChain.jsClassCall(
  "addBlock",
  proc(ctx: ptr JSContext; this_val: JSValue; argc: cint; argv: ptr UncheckedArray[JSValue]): JSValue {.cdecl.} =
    let
      chain = cast[CBChainPtr](this_val.getPtr(JsCBChain.classId))
      blkJs = argv[0]
      blk = cast[ptr CBRuntimeBlock](argv[0].getPtr(JsCBRuntimeBlock.classId))
    if chain == nil: throwTypeError(ctx, "addBlock invalid chain")
    if blk == nil: throwTypeError(ctx, "addBlock invalid block")
    
    # Chain will own the block, remove the ptr from the js side, so it won't destroy it!
    blkJs.attachPtr(nil)
    
    # now add to the chain
    chain.add(blk),
  1
)

var JsCBNode = JsClass(
  finalizer: proc(self: JSValue; classId: uint32) =
    var node = self.getPtr(classId)
    if node != nil:
      destroyNode(cast[ptr CBNode](node)),
  
  constructor: proc(self: JSValue; context: ptr JSContext; argc: int; argv: ptr UncheckedArray[JSValue]) =
    var node = createNode()
    self.attachPtr(node)
)
jsClass(jsContext, "Node", JsCBNode)

JsCBNode.jsClassCall(
  "schedule",
  proc(ctx: ptr JSContext; this_val: JSValue; argc: cint; argv: ptr UncheckedArray[JSValue]): JSValue {.cdecl.} =
    let
      node = cast[ptr CBNode](this_val.getPtr(JsCBNode.classId))
      chain = cast[CBChainPtr](argv[0].getPtr(JsCBChain.classId))
    if node == nil: throwTypeError(ctx, "schedule invalid node")
    if chain == nil: throwTypeError(ctx, "schedule invalid chain")
    
    node.schedule(chain),
  1
)

when isMainModule:
  eval(jsContext, "{ var x = new Node(); var y = new Node(); x = y; y.testCall(); }")
  eval(jsContext, """
  var blk = new Block("Log")
  const blockName = blk.name
  echo(blockName)
  var chain = new Chain("mainChain")
  var node = new Node()
  chain.addBlock(blk)
  node.schedule(chain)
  """)


