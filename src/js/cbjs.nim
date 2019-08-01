import quicknimjs
import nimline
import config

importExtras()

var
  jsRuntime = newJSRuntime()
  jsContext = newJSContext(jsRuntime)

# let's expose global
jsContext.Global.setProperty(jsContext, "global", jsContext.Global)

proc loadBuiltIns() =
  var logProc = newProc(jsContext, proc(ctx: ptr JSContext; this_val: JSValue; argc: cint; argv: ptr UncheckedArray[JSValue]): JSValue {.cdecl.} =
    for i in 0..<argc:
      # Any JS object -> JS string -> Nim string -> cstring pointer
      let
        jsStr = argv[i].asJsStr(jsContext)
        cstr = jsStr.getCStr(jsContext)
      logs(cstr)
      cstr.freeCStr(jsContext)
    return Undefined
  , "log", 1)
  jsContext.Global.setProperty(jsContext, "log", logProc)

  var sleepProc = newProc(jsContext, proc(ctx: ptr JSContext; this_val: JSValue; argc: cint; argv: ptr UncheckedArray[JSValue]): JSValue {.cdecl.} =
    let sleepTime = argv[0].toFloat(jsContext)
    sleep(sleepTime)
    return Undefined
  , "sleep", 1)
  jsContext.Global.setProperty(jsContext, "sleep", sleepProc)
loadBuiltIns()

# Forwarding those here
var JsCBRuntimeBlock = JsClass(
  finalizer: proc(self: JSValue; classId: uint32) =
    var blk = cast[ptr CBRuntimeBlock](self.getPtr(classId))
    if blk != nil:
      blk[].cleanup(blk)
      blk[].destroy(blk),
  
  constructor: proc(self: var JSValue; argc: cint; argv: ptr UncheckedArray[JSValue]) =
    if not argv[0].isStr:
      self = throwTypeError(jsContext, "Block constructor requires a name")
      return
    
    let
      name = argv[0].getCStr(jsContext)
      blk = createBlock($name)
    name.freeCStr(jsContext)
    
    if blk == nil:
      self = throwTypeError(jsContext, "Block construction failed, this block does not exist.")
      return
    
    blk[].setup(blk)
    self.attachPtr(blk),
  
  constructorMinArgs: 1
)
jsClass(jsContext, "Block", JsCBRuntimeBlock)

var JsCBChain = JsClass(
  finalizer: proc(self: JSValue; classId: uint32) =
    var chain = cast[CBChainPtr](self.getPtr(classId))
    if chain != nil:
      destroy(chain),
  
  constructor: proc(self: var JSValue; argc: cint; argv: ptr UncheckedArray[JSValue]) =
    if not argv[0].isStr:
      self = throwTypeError(jsContext, "Chain constructor requires a name")
      return
    
    let
      name = argv[0].getCStr(jsContext)
      chain = newChain($name)
    name.freeCStr(jsContext)
    self.attachPtr(chain),
    
  
  constructorMinArgs: 1
)
jsClass(jsContext, "Chain", JsCBChain)

type 
  JsVarStorage {.pure.} = enum
    None,
    Copy,
    Seq
  
  JsCBVarBox = object
    value: CBVar
    storage: JsVarStorage

# A immutable CBVar
when true:
  var JsCBVarConst = JsClass(
    finalizer: proc(self: JSValue; classId: uint32) =
      discard,
    constructor: proc(self: var JSValue; argc: cint; argv: ptr UncheckedArray[JSValue]) =
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
      
      return Undefined
  )
  JsCBVarConst.jsClassCall(
    "toString",
    proc(ctx: ptr JSContext; this_val: JSValue; argc: cint; argv: ptr UncheckedArray[JSValue]): JSValue {.cdecl.} =
      let
        pvar = cast[ptr CBVar](this_val.getPtr(JsCBVarConst.classId))
      
      if pvar != nil:
        var strVal = $pvar[]
        return newStr(strVal.cstring, jsContext)
      
      return Undefined,
  )

  proc toConstJSValue*(v: var CBVar; context: ptr JSContext): JSValue {.inline.} =
    result = newClassObj(context, JsCBVarConst.classId, JsCBVarConst.prototype)
    result.attachPtr(addr v)

var JsCBVar: JSClass

proc jsToVar(dst: ptr JsCBVarBox; src: JsValue) =
  if src.isStr:
    let
      strVar = src.getCStr(jsContext)
    var strTemp: CBVar = strVar.CBString
    dst[].storage = JsVarStorage.Copy
    quickcopy(dst[].value, strTemp)
    strVar.freeCStr(jsContext)
  
  elif src.isInt:
    dst[].value.valueType = Int
    dst[].value.intValue = src.toInt(jsContext)
  
  elif src.isFloat:
    dst[].value.valueType = Float
    dst[].value.floatValue = src.toFloat(jsContext)
  
  elif src.isBool:
    dst[].value.valueType = Bool
    dst[].value.boolValue = src.toBool(jsContext)
  
  elif src.isArray(jsContext):
    dst[].value.valueType = Seq
    dst[].storage = JsVarStorage.Seq
    initSeq(dst[].value.seqValue)
    
    let
      jlen = src.getProperty(jsContext, "length")
      len = jlen.toInt32(jsContext)
    
    for i in 0..<len:
      let val = src.getProperty(jsContext, i)
      
      let blockValue = cast[ptr CBRuntimeBlock](val.getPtr(JsCBRuntimeBlock.classId))
      if blockValue != nil:
        # The block will own the block, remove the ptr from the js side, so it won't destroy it!
        val.attachPtr(nil)
        dst[].value.seqValue.push(blockValue)
        val.decRef(jsContext)
        continue
      
      # Finally try to wrap in a Const()
      let
        constStr = newStr("Const", jsContext)
        constBlkJs = JsCBRuntimeBlock.construct(jsContext, [constStr])
        constBlk = cast[ptr CBRuntimeBlock](constBlkJs.getPtr(JsCBRuntimeBlock.classId))
      
      if constBlk != nil:
        # The block will own the block, remove the ptr from the js side, so it won't destroy it!
        constBlkJs.attachPtr(nil)
        
        var cbvar = cast[ptr JsCBVarBox](val.getPtr(JsCBVar.classId))
        if cbvar != nil:
          constBlk[].setParam(constBlk, 0, cbvar[].value)
        else:
          # Set the var as param
          var tmp: JsCBVarBox
          jsToVar(addr tmp, val)
          constBlk[].setParam(constBlk, 0, tmp.value)
          # free mem from the temp if we need to
          case tmp.storage
          of JsVarStorage.None:
            discard
          of JsVarStorage.Copy:
            `~quickcopy` tmp.value
          of JsVarStorage.Seq:
            for sub in tmp.value.seqValue.mitems:
              `~quickcopy` sub
            freeSeq(tmp.value.seqValue)
          
        dst[].value.seqValue.push(constBlk)
        val.decRef(jsContext)
        continue
      
      val.decRef(jsContext)
    
  # Make sure this is the last as all is an obj, well at least array is
  elif src.isObj:
    let constVar = cast[ptr CBVar](src.getPtr(JsCBVarConst.classId))
    if constVar != nil:
      dst[].storage = JsVarStorage.Copy
      quickcopy(dst[].value, constVar[])
      return
    
    let blockValue = cast[ptr CBRuntimeBlock](src.getPtr(JsCBRuntimeBlock.classId))
    if blockValue != nil:
      dst[].value.valueType = Block
      dst[].value.blockValue = blockValue
      return
    
    let chainValue = cast[CBChainPtr](src.getPtr(JsCBChain.classId))
    if chainValue != nil:
      dst[].value.valueType = Chain
      dst[].value.chainValue = chainValue
      return

# A mutable CBVar
when true:
  JsCBVar = JsClass(
    finalizer: proc(self: JSValue; classId: uint32) =
      var cbvar = cast[ptr JsCBVarBox](self.getPtr(classId))
      if cbvar != nil:
        case cbvar.storage
        of JsVarStorage.None:
          discard
        of JsVarStorage.Copy:
          `~quickcopy` cbvar[].value
        of JsVarStorage.Seq:
          for val in cbvar[].value.seqValue.mitems:
            `~quickcopy` val
          freeSeq(cbvar[].value.seqValue)
        
        cppdel(cbvar),
    
    constructor: proc(self: var JSValue; argc: cint; argv: ptr UncheckedArray[JSValue]) =
      var newVar: ptr JsCBVarBox
      cppnew(newVar, JsCBVarBox, JsCBVarBox)
      
      # Add logic to init var from any value...
      if argc == 1:
        jsToVar(newVar, argv[0])
      
      elif argc > 1:
        if argv[0].isBool: # First bool, true = floats, false = ints
          let isFloat = argv[0].toBool(jsContext)
          if not isFloat: # infer from first arg
            case argc
            of 2:
              newVar[].value.valueType = Int
              newVar[].value.intValue = argv[1].toInt(jsContext)
            of 3:
              newVar[].value.valueType = Int2
              for i in 1..<argc: newVar[].value.int2Value[i-1] = argv[i].toInt(jsContext)
            of 4:
              newVar[].value.valueType = Int3
              for i in 1..<argc: newVar[].value.int3Value[i-1] = argv[i].toInt(jsContext)
            of 5:
              newVar[].value.valueType = Int4
              for i in 1..<argc: newVar[].value.int4Value[i-1] = argv[i].toInt(jsContext)
            of 9:
              newVar[].value.valueType = Int8
              for i in 1..<argc: newVar[].value.int8Value[i-1] = argv[i].toInt(jsContext)
            of 17:
              newVar[].value.valueType = Int16
              for i in 1..<argc: newVar[].value.int16Value[i-1] = argv[i].toInt(jsContext)
            else:
              self = throwTypeError(jsContext, "Int variable out of range, pass as an array instead.")
              return
          else: # infer from first arg
            case argc
            of 2:
              newVar[].value.valueType = Float
              newVar[].value.floatValue = argv[1].toFloat(jsContext)
            of 3:
              newVar[].value.valueType = Float2
              for i in 1..<argc: newVar[].value.float2Value[i-1] = argv[i].toFloat(jsContext)
            of 4:
              newVar[].value.valueType = Float3
              for i in 1..<argc: newVar[].value.float3Value[i-1] = argv[i].toFloat(jsContext)
            of 5:
              newVar[].value.valueType = Float4
              for i in 1..<argc: newVar[].value.float4Value[i-1] = argv[i].toFloat(jsContext)
            else:
              self = throwTypeError(jsContext, "Float variable out of range, pass as an array instead.")
              return
      
      self.attachPtr(newVar),
    
    constructorMinArgs: 0
  )
  jsClass(jsContext, "Var", JsCBVar)

  JsCBVar.jsClassCall(
    "toString",
    proc(ctx: ptr JSContext; this_val: JSValue; argc: cint; argv: ptr UncheckedArray[JSValue]): JSValue {.cdecl.} =
      let
        pvar = cast[ptr CBVar](this_val.getPtr(JsCBVar.classId))
      
      if pvar != nil:
        var strVal = $pvar[]
        return newStr(strVal.cstring, jsContext)
      
      return Undefined,
  )

  proc toJSValue*(v: var CBVar; context: ptr JSContext): JSValue {.inline.} =
    result = newClassObj(context, JsCBVar.classId, JsCBVar.prototype)
    var newVar: ptr JsCBVarBox
    cppnew(newVar, JsCBVarBox, JsCBVarBox)
    newVar[].storage = JsVarStorage.Copy
    quickcopy(newVar[].value, v)
    result.attachPtr(newVar)

# CBRuntimeBlock
when true:
  JsCBRuntimeBlock.jsClassGetSet(
    "name",
    proc(ctx: ptr JSContext; this_val: JSValue): JSValue {.cdecl.} =
      let
        blk = cast[ptr CBRuntimeBlock](this_val.getPtr(JsCBRuntimeBlock.classId))
      
      if blk != nil:
        let text = blk[].name(blk)
        return text.newStr(ctx)
      
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
      
      return Undefined
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
  JsCBRuntimeBlock.jsClassCall(
    "setParam",
    proc(ctx: ptr JSContext; this_val: JSValue; argc: cint; argv: ptr UncheckedArray[JSValue]): JSValue {.cdecl.} =
      let
        blk = cast[ptr CBRuntimeBlock](this_val.getPtr(JsCBRuntimeBlock.classId))
        idx = argv[0].toInt32(jsContext)
        inVar = cast[ptr CBVar](argv[1].getPtr(JsCBVar.classId))
      if inVar == nil:
        return throwTypeError(jsContext, "Invalid Var value passed")
      
      if blk != nil:
        let errors = blk.validate(idx, inVar[])
        for error in errors:
          if error.error:
            log $blk[].name(blk), " setParam failed with error: ", error.message
            return throwInternalError(jsContext, "setParam failed")
        
        blk[].setParam(blk, idx, inVar[])
      
      return Undefined,
    2
  )
  JsCBRuntimeBlock.jsClassCall(
    "getParam",
    proc(ctx: ptr JSContext; this_val: JSValue; argc: cint; argv: ptr UncheckedArray[JSValue]): JSValue {.cdecl.} =
      let
        blk = cast[ptr CBRuntimeBlock](this_val.getPtr(JsCBRuntimeBlock.classId))
        idx = argv[0].toInt32(jsContext)
      
      if blk != nil:
        var v = blk[].getParam(blk, idx)
        return v.toJSValue(jsContext)
      
      return Undefined,
    1
  )

# CBChain
when true:
  JsCBChain.jsClassCall(
    "addBlock",
    proc(ctx: ptr JSContext; this_val: JSValue; argc: cint; argv: ptr UncheckedArray[JSValue]): JSValue {.cdecl.} =
      let
        chain = cast[CBChainPtr](this_val.getPtr(JsCBChain.classId))
        blkJs = argv[0]
        blk = cast[ptr CBRuntimeBlock](argv[0].getPtr(JsCBRuntimeBlock.classId))
      if chain == nil: return throwTypeError(ctx, "addBlock invalid chain")
      if blk == nil: return throwTypeError(ctx, "addBlock invalid block")
      
      # Chain will own the block, remove the ptr from the js side, so it won't destroy it!
      blkJs.attachPtr(nil)
      
      # now add to the chain
      chain.add(blk)
      
      return Undefined,
    1
  )

  JsCBChain.jsClassGetSet(
    "looped",
    proc(ctx: ptr JSContext; this_val: JSValue): JSValue {.cdecl.} =
      let
        chain = cast[CBChainPtr](this_val.getPtr(JsCBChain.classId))
      
      if chain != nil:
        return chain[].looped.jsBool(ctx)
      
      return Undefined,
    proc(ctx: ptr JSContext; this_val: JSValue; value: JSValue): JSValue {.cdecl.} =
      let
        chain = cast[CBChainPtr](this_val.getPtr(JsCBChain.classId))
      
      if chain != nil and value.isBool:
        chain[].looped = value.toBool(jsContext)
      
      return Undefined
  )
  JsCBChain.jsClassGetSet(
    "unsafe",
    proc(ctx: ptr JSContext; this_val: JSValue): JSValue {.cdecl.} =
      let
        chain = cast[CBChainPtr](this_val.getPtr(JsCBChain.classId))
      
      if chain != nil:
        return chain[].unsafe.jsBool(ctx)
      
      return Undefined,
    proc(ctx: ptr JSContext; this_val: JSValue; value: JSValue): JSValue {.cdecl.} =
      let
        chain = cast[CBChainPtr](this_val.getPtr(JsCBChain.classId))
      
      if chain != nil and value.isBool:
        chain[].unsafe = value.toBool(jsContext)
      
      return Undefined
  )

# CBNode
when true:
  var JsCBNode = JsClass(
    finalizer: proc(self: JSValue; classId: uint32) =
      var node = self.getPtr(classId)
      if node != nil:
        destroyNode(cast[ptr CBNode](node)),
    
    constructor: proc(self: var JSValue; argc: cint; argv: ptr UncheckedArray[JSValue]) =
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
      if node == nil: return throwTypeError(ctx, "schedule invalid node")
      if chain == nil: return throwTypeError(ctx, "schedule invalid chain")
      
      # Store a reference to the chain as well!
      this_val.setProperty(jsContext, "_chain_" & chain[].name.c_str().to(cstring), argv[0].incRef(jsContext))
      
      try:
        node.schedule(chain)
      except StdException as e:
        return throwInternalError(jsContext, e.what())
      
      return Undefined,
    1
  )
  JsCBNode.jsClassCall(
    "tick",
    proc(ctx: ptr JSContext; this_val: JSValue; argc: cint; argv: ptr UncheckedArray[JSValue]): JSValue {.cdecl.} =
      let
        node = cast[ptr CBNode](this_val.getPtr(JsCBNode.classId))
      if node == nil: return throwTypeError(ctx, "schedule invalid node")
      
      node.tick()
      
      return Undefined,
    0
  )

when isMainModule:
  import parseopt, strutils, tables

  const reservedWords = ["break", "case", "catch", "class", "const", "continue",
    "debugger", "default", "delete", "do", "else", "export", "extends",
    "finally", "for", "function", "if", "import", "in", "instanceof", "new",
    "return", "super", "switch", "this", "throw", "try", "typeof", "var",
    "void", "while", "with", "yield", "enum", "implements", "interface",
    "let", "package", "private", "protected", "public", "static", "await",
    "abstract", "boolean", "byte", "char", "double", "final", "float", "goto",
    "int", "long", "native", "short", "synchronized", "throws", "transient",
    "volatile", "null", "true", "false"]
  
  proc writeHelp() = discard
  
  proc writeVersion() = discard

  proc processBlock(namespace, name, fullName: string): string =
    let
      blk = createBlock(fullName)
      params = blk.parameters(blk)
    
    var paramsString = ""
    for param in params:
      var pname = ($param.name).toLowerAscii
      if pname in reservedWords:
        pname = pname & "_"
      if paramsString == "":
        paramsString &= pname
      else:
        paramsString &= ", " & pname
    
    var extraSpace = ""
    if namespace != "":
      extraSpace = "  "
    
    if namespace != "":
      result &= "  static $#($#) {\n" % [name, paramsString]
    else:
      result &= "export var $# = function($#) {\n" % [name, paramsString]
    
    result &= "$#  let blk = new Block(\"$#\")\n" % [extraSpace, fullName]
    
    var pindex = 0
    for param in params:
      var pname = ($param.name).toLowerAscii
      if pname in reservedWords:
        pname = pname & "_"
      result &= "$#  if($# !== undefined) {\n" % [extraSpace, pname]
      result &= "$#    if($# instanceof Var)\n" % [extraSpace, pname]
      result &= "$#      blk.setParam($#, $#)\n" % [extraSpace, $pindex, pname]
      result &= "$#    else\n" % [extraSpace]
      result &= "$#      blk.setParam($#, new Var($#))\n" % [extraSpace, $pindex, pname]
      result &= "$#  }\n" % [extraSpace]
      inc pindex
    
    result &= "$#  return blk\n" % [extraSpace]
    result &= "$#}\n\n" % [extraSpace]

    blk.cleanup(blk)
    blk.destroy(blk)

  proc generateSugar(): string =
    var
      output = ""
      blocks = cbBlocks()
      namespaces = initTable[string, string]()
    
    for blkName in blocks:
      let
        namesplit = blkName.split(".")
      if namesplit.len == 1: # No namespace
        output &= processBlock("", namesplit[0], blkName)
      elif namesplit.len == 2:
        if namesplit[0] notin namespaces:
          var namespaceName = namesplit[0]
          if namespaceName in reservedWords:
            namespaceName = namespaceName & "_"
          namespaces[namesplit[0]] = "class $# {\n" % [namespaceName]
        namespaces[namesplit[0]] &= processBlock(namesplit[0], namesplit[1], blkName)
    
    for _, definition in namespaces.mpairs:
      definition &= "}\n\n"
      output &= definition

    output &= """
function chain(blocks, name, looped, unsafe) {
  if(name === undefined) {
    name = "unnamed chain"
  }
  
  let newChain = new Chain(name)
  
  if(looped === undefined) {
    looped = false
  }
  newChain.looped = looped

  if(unsafe === undefined) {
    unsafe = false
  }
  newChain.unsafe = unsafe

  blocks.forEach(function(val) {
    if(val instanceof Block)
      newChain.addBlock(val)
    else // assume var
      newChain.addBlock(Const(val))
  })

  return newChain
}

function Int(x)
{
  return new Var(false, x)
}
function Int2(x, y)
{
  return new Var(false, x, y)
}
function Int3(x, y, z)
{
  return new Var(false, x, y, z)
}
function Int4(x, y, z, w)
{
  return new Var(false, x, y, z, w)
}

function Float(x)
{
  return new Var(true, x)
}
function Float2(x, y)
{
  return new Var(true, x, y)
}
function Float3(x, y, z)
{
  return new Var(true, x, y, z)
}
function Float4(x, y, z, w)
{
  return new Var(true, x, y, z, w)
}

"""

    output &= "export { chain, Int, Int2, Int3, Int4, Float, Float2, Float3, Float4"
    for namespace, _ in namespaces.mpairs:
      output &= ", $#" % [namespace]
    output &= " };\n\n"
    
    return output
  
  proc loadBlocks() =
    # Load blocks as module
    let
      res = eval(jsContext, generateSugar(), "blocks", true)
    if res.isException:
      let
        ex = getLastException(jsContext)
        exStr = ex.asJsStr(jsContext).getCStr(jsContext)
      if exStr != "": logs(exStr)
      exStr.freeCStr(jsContext)

  when defined cbjsinject:
    import os
    
    loadBlocks()
    let filename = getEnv("CBJS_ATTACH")
    log("Loading file: " & filename)
    let
      res = eval(jsContext, readFile(filename), filename, true)
    if res.isException:
      let
        ex = getLastException(jsContext)
        exStr = ex.asJsStr(jsContext).getCStr(jsContext)
      if exStr != "": logs(exStr)
      exStr.freeCStr(jsContext)
    
  else:
    loadBlocks()
    
    var filename: string
    
    for kind, key, val in getopt():
      case kind
      of cmdArgument:
        filename = key
      of cmdLongOption, cmdShortOption:
        case key
        of "help", "h": writeHelp()
        of "version", "v": writeVersion()
        of "dump", "d": writeFile("blocks.js", generateSugar())
      of cmdEnd: assert(false) # cannot happen
    if filename == "":
      # no filename has been given, so we show the help
      writeHelp()
    else:
      let
        res = eval(jsContext, readFile(filename), filename, true)
      if res.isException:
        let
          ex = getLastException(jsContext)
          exStr = ex.asJsStr(jsContext).getCStr(jsContext)
        if exStr != "": logs(exStr)
        exStr.freeCStr(jsContext)
