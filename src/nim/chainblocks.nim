# ChainBlocks
# Inspired by apple Shortcuts actually..
# Simply define a program by chaining blocks (black boxes), that have input and output/s

import sequtils, tables, times, macros, strutils
import fragments/[serialization]
import images, packedjson

import types
export types

import nimline

when appType != "lib":
  {.compile: "../core/chainblocks.cpp".}
  {.passL: "-lboost_context-mt".}
  {.passC: "-DCHAINBLOCKS_RUNTIME.".}
else:
  {.emit: """/*INCLUDESECTION*/
#define DG_DYNARR_IMPLEMENTATION 1
#define GB_STRING_IMPLEMENTATION 1
""".}

const
  FragCC* = toFourCC("frag")

proc elems*(v: CBInt2): array[2, int64] {.inline.} = [v.toCpp[0].to(int64), v.toCpp[1].to(int64)]
proc elems*(v: CBInt3): array[3, int64] {.inline.} = [v.toCpp[0].to(int64), v.toCpp[1].to(int64), v.toCpp[2].to(int64)]
proc elems*(v: CBInt4): array[4, int64] {.inline.} = [v.toCpp[0].to(int64), v.toCpp[1].to(int64), v.toCpp[2].to(int64), v.toCpp[3].to(int64)]
proc elems*(v: CBFloat2): array[2, float64] {.inline.} = [v.toCpp[0].to(float64), v.toCpp[1].to(float64)]
proc elems*(v: CBFloat3): array[3, float64] {.inline.} = [v.toCpp[0].to(float64), v.toCpp[1].to(float64), v.toCpp[2].to(float64)]
proc elems*(v: CBFloat4): array[4, float64] {.inline.} = [v.toCpp[0].to(float64), v.toCpp[1].to(float64), v.toCpp[2].to(float64), v.toCpp[3].to(float64)]

proc makeStringVar*(s: string): CBVar {.inline.} =
  result.valueType = String
  result.stringValue = newString(s)

converter toCBTypesInfo*(s: tuple[types: set[CBType]; canBeSeq: bool]): CBTypesInfo {.inline, noinit.} =
  initSeq(result)
  for t in s.types:
    global.da_push(result, CBTypeInfo(basicType: t, sequenced: s.canBeSeq)).to(void)

converter toCBTypesInfo*(s: set[CBType]): CBTypesInfo {.inline, noinit.} = (s, false)

converter toCBTypeInfo*(s: tuple[ty: CBType, isSeq: bool]): CBTypeInfo {.inline, noinit.} =
  CBTypeInfo(basicType: s.ty, sequenced: s.isSeq)

converter toCBTypeInfo*(s: CBType): CBTypeInfo {.inline, noinit.} = (s, false)

converter toCBTypesInfo*(s: seq[CBTypeInfo]): CBTypesInfo {.inline.} =
  initSeq(result)
  for ti in s:
    result.push ti

converter toCBParameterInfo*(record: tuple[paramName: string; helpText: string; actualTypes: tuple[types: set[CBType]; canBeSeq: bool]; usesContext: bool]): CBParameterInfo {.inline.} =
  # This relies on const strings!! will crash if not!
  result.valueTypes = record.actualTypes.toCBTypesInfo()
  let
    pname = record.paramName
    help = record.helpText
  result.name = pname
  result.help = help
  result.allowContext = record.usesContext

converter toCBParameterInfo*(s: tuple[paramName: string; actualTypes: set[CBType]]): CBParameterInfo {.inline, noinit.} = (s.paramName, "", (s.actualTypes, false), false)
converter toCBParameterInfo*(s: tuple[paramName: string; helpText: string; actualTypes: set[CBType]]): CBParameterInfo {.inline, noinit.} = (s.paramName, s.helpText, (s.actualTypes, false), false)
converter toCBParameterInfo*(s: tuple[paramName: string; helpText: string; actualTypes: set[CBType]; usesContext: bool]): CBParameterInfo {.inline, noinit.} = (s.paramName, s.helpText, (s.actualTypes, false), s.usesContext)
converter toCBParameterInfo*(s: tuple[paramName: string; actualTypes: set[CBType], usesContext: bool]): CBParameterInfo {.inline, noinit.} = (s.paramName, "", (s.actualTypes, false), s.usesContext)
converter toCBParameterInfo*(s: tuple[paramName: string; actualTypes: tuple[types: set[CBType]; canBeSeq: bool]]): CBParameterInfo {.inline, noinit.} = (s.paramName, "", (s.actualTypes.types, s.actualTypes.canBeSeq), false)
converter toCBParameterInfo*(s: tuple[paramName: string; actualTypes: tuple[types: set[CBType]; canBeSeq: bool]; usesContext: bool]): CBParameterInfo {.inline, noinit.} = (s.paramName, "", (s.actualTypes.types, s.actualTypes.canBeSeq), s.usesContext)

converter cbParamsToSeq*(params: var CBParametersInfo): seq[CBParameterInfo] =
  for item in params.items:
    result.add item

converter toCBTypesInfoTuple*(s: tuple[a: string; b: set[CBType]]): tuple[c: string; d: CBTypesInfo] {.inline, noinit.} =
  result[0] = s.a
  result[1] = s.b

converter toCBParametersInfo*(s: seq[tuple[paramName: string; helpText: string; actualTypes: tuple[types: set[CBType]; canBeSeq: bool]; usesContext: bool]]): CBParametersInfo {.inline.} =
  initSeq(result)
  for i in 0..s.high:
    var record = s[i]
    result.push(record.toCBParameterInfo())

converter toCBParametersInfo*(s: seq[tuple[paramName: string; actualTypes: set[CBType]]]): CBParametersInfo {.inline, noinit.} =
  initSeq(result)
  for i in 0..s.high:
    var record = s[i]
    result.push(record.toCBParameterInfo())

converter toCBParametersInfo*(s: seq[tuple[paramName: string; helpText: string; actualTypes: set[CBType]]]): CBParametersInfo {.inline.} =
  initSeq(result)
  for i in 0..s.high:
    var record = s[i]
    result.push(record.toCBParameterInfo())

converter toCBParametersInfo*(s: seq[tuple[paramName: string; actualTypes: set[CBType]; usesContext: bool]]): CBParametersInfo {.inline.} =
  initSeq(result)
  for i in 0..s.high:
    var record = s[i]
    result.push(record.toCBParameterInfo())

converter toCBParametersInfo*(s: seq[tuple[paramName: string; helpText: string; actualTypes: set[CBType]; usesContext: bool]]): CBParametersInfo {.inline.} =
  initSeq(result)
  for i in 0..s.high:
    var record = s[i]
    result.push(record.toCBParameterInfo())

converter toCBParametersInfo*(s: seq[tuple[paramName: string; actualTypes: tuple[types: set[CBType]; canBeSeq: bool]]]): CBParametersInfo {.inline.} =
  initSeq(result)
  for i in 0..s.high:
    var record = s[i]
    result.push(record.toCBParameterInfo())

converter toCBParametersInfo*(s: seq[tuple[paramName: string; actualTypes: tuple[types: set[CBType]; canBeSeq: bool]; usesContext: bool]]): CBParametersInfo {.inline.} =
  initSeq(result)
  for i in 0..s.high:
    var record = s[i]
    result.push(record.toCBParameterInfo())

converter toCBParametersInfo*(s: seq[tuple[paramName: string; actualTypes: seq[CBTypeInfo]]]): CBParametersInfo {.inline.} =
  initSeq(result)
  for i in 0..s.high:
    var
      record = s[i]
      pinfo = CBParameterInfo(name: record.paramName)
    initSeq(pinfo.valueTypes)
    for at in record.actualTypes:
      pinfo.valueTypes.push(at)
    result.push(pinfo)

when isMainModule:
  var
    tys = { Int, Int4, Float3 }
    vtys = tys.toCBTypesInfo()
  assert vtys.len() == 3
  assert vtys[0].basicType == Int
  assert vtys[1].basicType == Int4
  assert vtys[2].basicType == Float3

  var
    tys2 = ({ Int4, Float3 }, true)
    vtys2 = tys2.toCBTypesInfo()
  assert vtys2.len() == 2
  assert vtys2[0].basicType == Int4
  assert vtys2[1].basicType == Float3
  assert vtys2[0].sequenced

  var
    pi1 = ("Param1", { Int, Int4 }).toCBParameterInfo()
  assert pi1.name == "Param1"
  assert pi1.valueTypes[0].basicType == Int
  assert pi1.valueTypes[1].basicType == Int4
  assert not pi1.allowContext

  var
    pi2 = ("Param2", { Int, Int4 }, true).toCBParameterInfo()
  assert pi2.name == "Param2"
  assert pi2.valueTypes[0].basicType == Int
  assert pi2.valueTypes[1].basicType == Int4
  assert pi2.allowContext

  var
    pis1 = @[
      ("Key", { Int }),
      ("Ctrl", { Bool }),
      ("Alt", { Bool }),
      ("Shift", { Bool }),
    ].toCBParametersInfo()
  assert pis1[0].name == "Key"
  assert pis1[0].valueTypes[0].basicType == Int
  assert pis1[1].name == "Ctrl"
  assert pis1[1].valueTypes[0].basicType == Bool

  var
    pis2 = @[("Window", "A window name or variable if we wish to send the event only to a target window.", { String })].toCBParametersInfo()
  assert pis2[0].name == "Window"
  assert pis2[0].help == "A window name or variable if we wish to send the event only to a target window."
  assert pis2[0].valueTypes[0].basicType == String

template iterateTuple(t: tuple; storage: untyped; castType: untyped): untyped =
  var idx = 0
  for x in t.fields:
    storage.toCpp[idx] = x.castType
    inc idx

converter toInt64Value*(v: CBVar): int64 {.inline.} =
  assert v.valueType == Int
  return v.intValue

converter toIntValue*(v: CBVar): int {.inline.} =
  assert v.valueType == Int
  return v.intValue.int

converter toFloatValue*(v: CBVar): float64 {.inline.} =
  assert v.valueType == Float
  return v.floatValue

converter toBoolValue*(v: CBVar): bool {.inline.} =
  assert v.valueType == Bool
  return v.boolValue

converter toCBVar*(cbSeq: CBSeq): CBVar {.inline.} =
  result.valueType = Seq
  result.seqValue = cbSeq

converter toCBVar*(cbStr: CBString): CBVar {.inline.} =
  result.valueType = String
  result.stringValue = cbStr

converter imageTypesConv*(cbImg: CBImage): Image[uint8] {.inline, noinit.} =
  result.width = cbImg.width
  result.height = cbImg.height
  result.channels = cbImg.channels
  result.data = cbImg.data

converter imageTypesConv*(cbImg: Image[uint8]): CBImage {.inline, noinit.} =
  result.width = cbImg.width.int32
  result.height = cbImg.height.int32
  result.channels = cbImg.channels.int32
  result.data = cbImg.data

proc asCBVar*(v: pointer; vendorId, typeId: FourCC): CBVar {.inline.} =
  return CBVar(valueType: Object, objectValue: v, objectVendorId: vendorId.int32, objectTypeId: typeId.int32)

converter toCBVar*(v: int): CBVar {.inline.} =
  return CBVar(valueType: Int, intValue: v)

converter toCBVar*(v: int64): CBVar {.inline.} =
  return CBVar(valueType: Int, intValue: v)

converter toCBVar*(v: CBInt2): CBVar {.inline.} =
  return CBVar(valueType: Int2, int2Value: v)

converter toCBVar*(v: CBInt3): CBVar {.inline.} =
  return CBVar(valueType: Int3, int3Value: v)

converter toCBVar*(v: CBInt4): CBVar {.inline.} =
  return CBVar(valueType: Int4, int4Value: v)

converter toCBVar*(v: tuple[a,b: int]): CBVar {.inline.} =
  var wval: CBInt2
  iterateTuple(v, wval, int64)
  return CBVar(valueType: Int2, int2Value: wval)

converter toCBVar*(v: tuple[a,b: int64]): CBVar {.inline.} =
  var wval: CBInt2
  iterateTuple(v, wval, int64)
  return CBVar(valueType: Int2, int2Value: wval)

converter toCBVar*(v: tuple[a,b,c: int]): CBVar {.inline.} =
  var wval: CBInt3
  iterateTuple(v, wval, int64)
  return CBVar(valueType: Int3, int3Value: wval)

converter toCBVar*(v: tuple[a,b,c: int64]): CBVar {.inline.} =
  var wval: CBInt3
  iterateTuple(v, wval, int64)
  return CBVar(valueType: Int3, int3Value: wval)

converter toCBVar*(v: tuple[a,b,c,d: int]): CBVar {.inline.} =
  var wval: CBInt4
  iterateTuple(v, wval, int64)
  return CBVar(valueType: Int4, int4Value: wval)

converter toCBVar*(v: tuple[a,b,c,d: int64]): CBVar {.inline.} =
  var wval: CBInt4
  iterateTuple(v, wval, int64)
  return CBVar(valueType: Int4, int4Value: wval)

converter toCBVar*(v: bool): CBVar {.inline.} =
  return CBVar(valueType: Bool, boolValue: v)

converter toCBVar*(v: float32): CBVar {.inline.} =
  return CBVar(valueType: Float, floatValue: v)

converter toCBVar*(v: float64): CBVar {.inline.} =
  return CBVar(valueType: Float, floatValue: v)

converter toCBVar*(v: CBFloat2): CBVar {.inline.} =
  return CBVar(valueType: Float2, float2Value: v)

converter toCBVar*(v: CBFloat3): CBVar {.inline.} =
  return CBVar(valueType: Float3, float3Value: v)

converter toCBVar*(v: CBFloat4): CBVar {.inline.} =
  return CBVar(valueType: Float4, float4Value: v)

converter toCBVar*(v: tuple[a,b: BiggestFloat]): CBVar {.inline.} =
  var wval: CBFloat2
  iterateTuple(v, wval, float64)
  return CBVar(valueType: Float2, float2Value: wval)

converter toCBVar*(v: tuple[a,b,c: BiggestFloat]): CBVar {.inline.} =
  var wval: CBFloat3
  iterateTuple(v, wval, float64)
  return CBVar(valueType: Float3, float3Value: wval)

converter toCBVar*(v: tuple[a,b,c,d: BiggestFloat]): CBVar {.inline.} =
  var wval: CBFloat4
  iterateTuple(v, wval, float64)
  return CBVar(valueType: Float4, float4Value: wval)

converter toCBVar*(v: CBBoolOp): CBVar {.inline.} =
  return CBVar(valueType: BoolOp, boolOpValue: v)

converter toCBVar*(v: tuple[r,g,b,a: uint8]): CBVar {.inline.} =
  return CBVar(valueType: Color, colorValue: CBColor(r: v.r, g: v.g, b: v.b, a: v.a))

converter toCBVar*(v: ptr CBChain): CBVar {.inline.} =
  return CBVar(valueType: Chain, chainValue: v)

converter toCBVar*(v: CBString): CBVar {.inline.} =
  return CBVar(valueType: String, stringValue: v)

template contextOrPure*(subject, container: untyped; wantedType: CBType; typeGetter: untyped): untyped =
  if container.valueType == ContextVar:
    var foundVar = context.variables.getOrDefault(container.stringValue)
    if foundVar.valueType == wantedType:
      subject = foundVar.typeGetter
  else:
    subject = container.typeGetter

var
  Empty* = CBVar(valueType: None, chainState: Continue)
  RestartChain* = CBVar(valueType: None, chainState: Restart)
  StopChain* = CBVar(valueType: None, chainState: Stop)

include ops

proc contextVariable*(name: string): CBVar {.inline.} =
  return CBVar(valueType: ContextVar, stringValue: name)

template `~~`*(name: string): CBVar = contextVariable(name)

template withChain*(chain, body: untyped): untyped =
  var `chain` {.inject.} = initChain()
  var prev = getCurrentChain()
  setCurrentChain(chain)
  body
  setCurrentChain(prev)

# Make those optional
template help*(b: auto): CBString = ""
template setup*(b: auto) = discard
template destroy*(b: auto) = discard
template preChain*(b: auto; context: CBContext) = discard
template postChain*(b: auto; context: CBContext) = discard
template exposedVariables*(b: auto): CBParametersInfo = @[]
template consumedVariables*(b: auto): CBParametersInfo = @[]
template parameters*(b: auto): CBParametersInfo = @[]
template setParam*(b: auto; index: int; val: CBVar) = discard
template getParam*(b: auto; index: int): CBVar = CBVar(valueType: None)
template cleanup*(b: auto) = discard

proc createBlock*(name: CBString): ptr CBRuntimeBlock {.importcpp: "chainblocks::createBlock(#)", header: "chainblocks.hpp".}
proc createBlock*(name: string): ptr CBRuntimeBlock = createBlock(cast[CBstring](name.cstring))

proc getCurrentChain*(): var CBChain {.importcpp: "chainblocks::getCurrentChain()", header: "chainblocks.hpp".}
proc setCurrentChain*(chain: ptr CBChain) {.importcpp: "chainblocks::setCurrentChain(#)", header: "chainblocks.hpp".}
proc setCurrentChain*(chain: var CBChain) {.importcpp: "chainblocks::setCurrentChain(#)", header: "chainblocks.hpp".}
proc add*(chain: var CBChain; blk: ptr CBRuntimeBlock) {.importcpp: "#.addBlock(#)", header: "chainblocks.hpp".}

proc globalVariable*(name: CBString): ptr CBVar {.importcpp: "chainblocks::globalVariable(#)", header: "chainblocks.hpp".}
proc globalVariable*(name: string): ptr CBVar {.inline.} =
  let cstr = CBString.cppinit(name)
  globalVariable(cstr)
proc hasGlobalVariable*(name: CBString): bool {.importcpp: "chainblocks::hasGlobalVariable(#)", header: "chainblocks.hpp".}
proc hasGlobalVariable*(name: string): bool {.inline.} =
  let cstr = CBString.cppinit(name)
  hasGlobalVariable(cstr)

proc startInternal(chain: ptr CBChain; loop: bool = false) {.importcpp: "chainblocks::start(#, #)", header: "chainblocks.hpp".}
proc start*(chain: ptr CBChain; loop: bool = false) {.inline.} =
  var frame = getFrameState()
  startInternal(chain, loop)
  setFrameState(frame)
proc start*(chain: var CBChain; loop: bool = false) {.inline.} =
  var chainptr = addr chain
  chainptr.start(loop)
proc tickInternal(chain: ptr CBChain; rootInput: CBVar = Empty) {.importcpp: "chainblocks::tick(#, #)", header: "chainblocks.hpp".}
proc tick*(chain: ptr CBChain; rootInput: CBVar = Empty) {.inline.} =
  var frame = getFrameState()
  tickInternal(chain, rootInput)
  setFrameState(frame)
proc tick*(chain: var CBChain; rootInput: CBVar = Empty) {.inline.} =
  var chainptr = addr chain
  chainptr.tick(rootInput)
proc stopInternal(chain: ptr CBChain): CBVar {.importcpp: "chainblocks::stop(#)", header: "chainblocks.hpp".}
proc stop*(chain: ptr CBChain): CBVar {.inline.} =
  var frame = getFrameState()
  result = stopInternal(chain)
  setFrameState(frame)
proc stop*(chain: var CBChain): CBVar {.inline.} =
  var chainptr = addr chain
  chainptr.stop()

var
  compileTimeMode {.compileTime.}: bool = false
  staticChainBlocks {.compileTime.}: seq[tuple[blk: NimNode; params: seq[NimNode]]]

proc generateInitMultiple*(rtName: string; ctName: typedesc; args: seq[tuple[label, value: NimNode]]): NimNode {.compileTime.} =
  if not compileTimeMode:
    # Generate vars
    # Setup
    # Validate params
    # Fill params
    # Add to chain
    var
      sym = gensym(nskVar)
      paramNames = gensym(nskVar)
      paramSets = gensym(nskVar)
    result = quote do:
      var
        `sym` = createBlock(`rtName`)
      `sym`.setup(`sym`)
      var
        cparams = `sym`.parameters(`sym`)
        paramsSeq = cparams.cbParamsToSeq()
        `paramNames` = paramsSeq.map do (x: CBParameterInfo) -> string: x.name.toLowerAscii
        `paramSets` = paramsSeq.map do (x: CBParameterInfo) -> CBTypesInfo: x.valueTypes
    
    for i in 0..`args`.high:
      var
        val = `args`[i].value
        lab = newStrLitNode($`args`[i].label.strVal)
      result.add quote do:
        var
          cbVar: CBVar = `val`
          label = `lab`.toLowerAscii
          idx = `paramNames`.find(label)
        assert idx != -1, "Could not find parameter: " & label
        # TODO
        # assert cbVar.valueType in `paramSets`[idx]
        `sym`.setParam(`sym`, idx, cbVar)
    
    result.add quote do:
      getCurrentChain().add(`sym`)
  else:
    var
      sym = gensym(nskVar)
      params: seq[NimNode]
    for arg in args:
      params.add arg.value
    result = quote do:
      var `sym`: `ctName`
    staticChainBlocks.add((sym, params))
    
proc generateInitSingle*(rtName: string; ctName: typedesc; args: NimNode): NimNode {.compileTime.} =
  if not compileTimeMode:
    result = quote do:
      var
        b = createBlock(`rtName`)
      b.setup(b)
      
      when not defined release:
        var
          # params = b.parameters(b)
          cbVar: CBVar = `args`
        # TODO
        # if params.len > 0:
        #   assert cbVar.valueType in params[0].valueType
      
      b.setParam(b, 0, cbVar)
      getCurrentChain().add(b)
  else:
    var
      sym = gensym(nskVar)
      params = @[args]
    result = quote do:
      var `sym`: `ctName`
    staticChainBlocks.add((sym, params))

proc generateInit*(rtName: string; ctName: typedesc): NimNode {.compileTime.} =
  if not compileTimeMode:
    result = quote do:
      var
        b = createBlock(`rtName`)
      b.setup(b)
      getCurrentChain().add(b)
  else:
    var
      sym = gensym(nskVar)
      params: seq[NimNode] = @[]
    result = quote do:
      var `sym`: `ctName`
    staticChainBlocks.add((sym, params))

proc allocateBlock*[T](): ptr T {.importcpp: "chainblocks::allocateBlock<'*0>()".}

when appType == "lib" and not defined(nimV2):
  template updateStackBottom*() =
    var sp {.volatile.}: pointer
    sp = addr(sp)
    nimGC_setStackBottom(sp)
    when compileOption("threads"):
      setupForeignThreadGC()
else:
  template updateStackBottom*() =
    discard

macro chainblock*(blk: untyped; blockName: string; namespaceStr: string = ""): untyped =
  var
    rtName = ident($blk & "RT")
    rtNameValue = ident($blk & "RTValue")
    macroName = ident($blockName)
    namespace = if $namespaceStr != "": $namespaceStr & "." else: ""

    nameProc = ident($blk & "_name")
    helpProc = ident($blk & "_help")

    setupProc = ident($blk & "_setup")
    destroyProc = ident($blk & "_destroy")
    
    preChainProc = ident($blk & "_preChain")
    postChainProc = ident($blk & "_postChain")
    
    inputTypesProc = ident($blk & "_inputTypes")
    outputTypesProc = ident($blk & "_outputTypes")
    
    exposedVariablesProc = ident($blk & "_exposedVariables")
    consumedVariablesProc = ident($blk & "_consumedVariables")
    
    parametersProc = ident($blk & "_parameters")
    setParamProc = ident($blk & "_setParam")
    getParamProc = ident($blk & "_getParam")
    
    activateProc = ident($blk & "_activate")
    cleanupProc = ident($blk & "_cleanup")
  
  result = quote do:
    import macros, strutils, sequtils

    type
      `rtNameValue` = object of CBRuntimeBlock
        sb: `blk`
      `rtName`* = ptr `rtNameValue`
    
    template name*(b: `blk`): string =
      (`namespace` & `blockName`)
    proc `nameProc`*(b: `rtName`): CBString {.cdecl.} =
      updateStackBottom()
      (`namespace` & `blockName`)
    proc `helpProc`*(b: `rtName`): CBString {.cdecl.} =
      updateStackBottom()
      b.sb.help()
    proc `setupProc`*(b: `rtName`) {.cdecl.} =
      updateStackBottom()
      b.sb.setup()
    proc `destroyProc`*(b: `rtName`) {.cdecl.} =
      updateStackBottom()
      b.sb.destroy()
    proc `preChainProc`*(b: `rtName`, context: CBContext) {.cdecl.} =
      updateStackBottom()
      b.sb.preChain(context)
    proc `postChainProc`*(b: `rtName`, context: CBContext) {.cdecl.} =
      updateStackBottom()
      b.sb.postChain(context)
    proc `inputTypesProc`*(b: `rtName`): CBTypesInfo {.cdecl.} =
      updateStackBottom()
      b.sb.inputTypes
    proc `outputTypesProc`*(b: `rtName`): CBTypesInfo {.cdecl.} =
      updateStackBottom()
      b.sb.outputTypes
    proc `exposedVariablesProc`*(b: `rtName`): CBParametersInfo {.cdecl.} =
      updateStackBottom()
      b.sb.exposedVariables
    proc `consumedVariablesProc`*(b: `rtName`): CBParametersInfo {.cdecl.} =
      updateStackBottom()
      b.sb.consumedVariables
    proc `parametersProc`*(b: `rtName`): CBParametersInfo {.cdecl.} =
      updateStackBottom()
      b.sb.parameters
    proc `setParamProc`*(b: `rtName`; index: int; val: CBVar) {.cdecl.} =
      updateStackBottom()
      b.sb.setParam(index, val)
    proc `getParamProc`*(b: `rtName`; index: int): CBVar {.cdecl.} =
      updateStackBottom()
      b.sb.getParam(index)
    proc `activateProc`*(b: `rtName`; context: CBContext; input: CBVar): CBVar {.cdecl.} =
      updateStackBottom()
      b.sb.activate(context, input)
    proc `cleanupProc`*(b: `rtName`) {.cdecl.} =
      updateStackBottom()
      b.sb.cleanup()
    registerBlock(`namespace` & `blockName`) do -> ptr CBRuntimeBlock {.cdecl.}:
      # we need to do this sorcery to ensure methods work!
      result = allocateBlock[`rtNameValue`]()
      result.name = cast[result.name.type](`nameProc`)
      result.help = cast[result.help.type](`helpProc`)
      result.setup = cast[result.setup.type](`setupProc`)
      result.destroy = cast[result.destroy.type](`destroyProc`)
      result.preChain = cast[result.preChain.type](`preChainProc`)
      result.postChain = cast[result.postChain.type](`postChainProc`)
      result.inputTypes = cast[result.inputTypes.type](`inputTypesProc`)
      result.outputTypes = cast[result.outputTypes.type](`outputTypesProc`)
      result.exposedVariables = cast[result.exposedVariables.type](`exposedVariablesProc`)
      result.consumedVariables = cast[result.consumedVariables.type](`consumedVariablesProc`)
      result.parameters = cast[result.parameters.type](`parametersProc`)
      result.setParam = cast[result.setParam.type](`setParamProc`)
      result.getParam = cast[result.getParam.type](`getParamProc`)
      result.activate = cast[result.activate.type](`activateProc`)
      result.cleanup = cast[result.cleanup.type](`cleanupProc`)

    static:
      echo "Registered chainblock: " & `namespace` & `blockName`
  
  if $namespaceStr != "":
    var nameSpaceType = ident($namespaceStr)
    # DSL Utilities
    result.add quote do:
      macro `macroName`*(_: typedesc[`nameSpaceType`]): untyped =
        result = generateInit(`namespace` & `blockName`, `blk`)

      macro `macroName`*(_: typedesc[`nameSpaceType`]; args: untyped): untyped =
        if args.kind == nnkStmtList:
          # We need to transform the stmt list
          var argNodes = newSeq[tuple[label, value: NimNode]]()
          for arg in args:
            assert arg.kind == nnkCall, "Syntax error" # TODO better errors
            var
              labelNode = arg[0]
              varNode = arg[1][0]
            argNodes.add (labelNode, varNode)
          result = generateInitMultiple(`namespace` & `blockName`, `blk`, argNodes)
        else:
          # assume single? maybe need to be precise to filter mistakes
          result = generateInitSingle(`namespace` & `blockName`, `blk`, args)
  else:
    result.add quote do:
      # DSL Utilities
      macro `macroName`*(): untyped =
        result = generateInit(`blockName`, `blk`)

      macro `macroName`*(args: untyped): untyped =
        if args.kind == nnkStmtList:
          # We need to transform the stmt list
          var argNodes = newSeq[tuple[label, value: NimNode]]()
          for arg in args:
            assert arg.kind == nnkCall, "Syntax error" # TODO better errors
            var
              labelNode = arg[0]
              varNode = arg[1][0]
            argNodes.add (labelNode, varNode)
          result = generateInitMultiple(`blockName`, `blk`, argNodes)
        else:
          # assume single? maybe need to be precise to filter mistakes
          result = generateInitSingle(`blockName`, `blk`, args)

when appType != "lib":
  # When building the runtime!

  proc registerBlock*(name: string; initProc: CBBlockConstructor) =
    invokeFunction("chainblocks::registerBlock", name, initProc).to(void)

  proc registerObjectType*(vendorId, typeId: FourCC; info: CBObjectInfo) =
    invokeFunction("chainblocks::registerObjectType", vendorId, typeId, info).to(void)

  proc getGlobalRegistry*(): pointer = invokeFunction("chainblocks::GetGlobalRegistry").to(pointer)

  proc contextVariable*(ctx: CBContext; name: CBString): ptr CBVar {.importcpp: "chainblocks::contextVariable(#, #)", header: "chainblocks.hpp".}
  proc contextVariable*(ctx: CBContext; name: string): ptr CBVar {.inline.} = contextVariable(ctx, cast[CBString](name.cstring))

  proc setError*(ctx: CBContext; errorTxt: CBString) {.importcpp: "(#->error = #)", header: "chainblocks.hpp".}

  proc initChain*(): CBChain {.inline, noinit.} = CBChain.cppinit()
  proc runChain*(chain: ptr CBChain, context: ptr CBContextObj; chainInput: CBVar): StdTuple2[bool, CBVar] {.importcpp: "chainblocks::runChain(#, #, #)", header: "chainblocks.hpp".}
  proc suspend*(seconds: float64): CBVar {.importcpp: "chainblocks::suspend(#)", header: "chainblocks.hpp".}
  
else:
  # When we are a dll with a collection of blocks!
  
  import dynlib
  
  type
    RegisterBlkProc = proc(registry: pointer; name: CBString; initProc: CBBlockConstructor) {.cdecl.}
    RegisterTypeProc = proc(registry: pointer; vendorId, typeId: FourCC; info: CBObjectInfo) {.cdecl.}
    CtxVariableProc = proc(ctx: CBContext; name: CBString): ptr CBVar {.cdecl.}
    CtxSetErrorProc = proc(ctx: CBContext; errorTxt: CBString) {.cdecl.}
    SuspendProc = proc(seconds: float64) {.cdecl.}
  
  var
    localBlocks: seq[tuple[name: string; initProc: CBBlockConstructor]]
    localObjTypes: seq[tuple[vendorId, typeId: FourCC; info: CBObjectInfo]]

    exeLib = loadLib()
    cbRegisterBlock = cast[RegisterBlkProc](exeLib.symAddr("chainblocks_RegisterBlock"))
    cbRegisterObjectType = cast[RegisterTypeProc](exeLib.symAddr("chainblocks_RegisterObjectType"))
    cbContextVar = cast[CtxVariableProc](exeLib.symAddr("chainblocks_ContextVariable"))
    cbSetError = cast[CtxSetErrorProc](exeLib.symAddr("chainblocks_SetError"))
    cbSuspend = cast[SuspendProc](exeLib.symAddr("chainblocks_Suspend"))

  proc chainblocks_init_module(registry: pointer) {.exportc, dynlib, cdecl.} =
    for localBlock in localBlocks:
      cbRegisterBlock(registry, localBlock.name, localBlock.initProc)
    
    for localType in localObjTypes:
      cbRegisterObjectType(registry, localType.vendorId, localType.typeId, localType.info)

  proc registerBlock*(name: string; initProc: CBBlockConstructor) =
    localBlocks.add((name, initProc))

  proc registerObjectType*(vendorId, typeId: FourCC; info: CBObjectInfo) =
    localObjTypes.add((vendorId, typeId, info))

  proc contextVariable*(ctx: CBContext; name: CBString): ptr CBVar {.inline.} = cbContextVar(ctx, name)
  proc setError*(ctx: CBContext; errorTxt: CBString) {.inline.} = cbSetError(ctx, errorTxt)
  
  proc suspend*(seconds: float64): CBVar {.inline.} = cbSuspend(seconds)

# This template is inteded to be used inside blocks activations
template pause*(secs: float): untyped =
  let
    suspendRes = suspend(secs.float64)
  if suspendRes.chainState != Continue:
    return suspendRes

template halt*(context: CBContext; txt: string): untyped =
  context.setError(txt)
  StopChain

proc block2Json*(blk: ptr CBRuntimeBlock): JsonNode

proc var2Json(v: CBVar): JsonNode =
  var jnode = newJObject()
  case v.valueType
    of Any, None, Object:
      jnode["t"] = %v.valueType
    of Bool:
      jnode["t"] = %v.valueType
      jnode["v"] = %v.boolValue
    of Int:
      jnode["t"] = %v.valueType
      jnode["v"] = %v.intValue
    of Int2:
      jnode["t"] = %v.valueType
      jnode["v"] = %[v.int2Value.toCpp[0].to(int64), v.int2Value.toCpp[1].to(int64)]
    of Int3:
      jnode["t"] = %v.valueType
      jnode["v"] = %[v.int3Value.toCpp[0].to(int64), v.int3Value.toCpp[1].to(int64), v.int3Value.toCpp[2].to(int64)]
    of Int4:
      jnode["t"] = %v.valueType
      jnode["v"] = %[v.int4Value.toCpp[0].to(int64), v.int4Value.toCpp[1].to(int64), v.int4Value.toCpp[2].to(int64), v.int4Value.toCpp[3].to(int64)] 
    of Float:
      jnode["t"] = %v.valueType
      jnode["v"] = %v.floatValue
    of Float2:
      jnode["t"] = %v.valueType
      jnode["v"] = %[v.float2Value.toCpp[0].to(float64), v.float2Value.toCpp[1].to(float64)]
    of Float3:
      jnode["t"] = %v.valueType
      jnode["v"] = %[v.float3Value.toCpp[0].to(float64), v.float3Value.toCpp[1].to(float64), v.float3Value.toCpp[2].to(float64)]
    of Float4:
      jnode["t"] = %v.valueType
      jnode["v"] = %[v.float4Value.toCpp[0].to(float64), v.float4Value.toCpp[1].to(float64), v.float4Value.toCpp[2].to(float64), v.float4Value.toCpp[3].to(float64)]
    of String, ContextVar:
      jnode["t"] = %v.valueType
      if v.stringValue.pointer != nil:
        jnode["v"] = %v.stringValue
      else:
        jnode["v"] = %("")
    of Color:
      jnode["t"] = %v.valueType
      echo v.colorValue.a
      jnode["v"] = %[v.colorValue.r.int, v.colorValue.g.int, v.colorValue.b.int, v.colorValue.a.int]
    of CBType.Image: # TODO/Figure, add context as param and stick the ref in the context
      jnode["t"] = %v.valueType
    of BoolOp:
      jnode["t"] = %v.valueType
      jnode["v"] = %v.boolOpValue
    of Seq:
      jnode["t"] = %v.valueType
      var arr = newJArray()
      for item in v.seqValue.items:
        arr.add var2Json(item)
      jnode["v"] = arr
    of Chain:
      discard
      # var arr = newJArray()
      # for b in v.chainValue.blocks.mitems:
      #   arr.add block2Json(b)
      # jnode["t"] = %v.valueType
      # jnode["v"] = arr
  return jnode

proc json2Var(node: var JsonNode): CBVar =
  let valueType = node["t"].getStr()
  case valueType
    of "Any":
      result = CBVar(valueType: Any)
    of "Object":
      result = CBVar(valueType: Object)
    of "None":
      result = CBVar(valueType: None)
    of "Bool":
      result = CBVar(valueType: Bool, boolValue: node["v"].getBool())
    of "Int":
      result = CBVar(valueType: Int, intValue: node["v"].getInt())
    of "Int2":
      result = (
          node["v"][0].getInt().int64, 
          node["v"][1].getInt().int64
        ).CBVar
    of "Int3":
      result = (
          node["v"][0].getInt().int64, 
          node["v"][1].getInt().int64, 
          node["v"][2].getInt().int64
        ).CBVar
    of "Int4":
      result = (
          node["v"][0].getInt().int64, 
          node["v"][1].getInt().int64, 
          node["v"][2].getInt().int64, 
          node["v"][3].getInt().int64
        ).CBVar
    of "Float":
      result = CBVar(valueType: Float, floatValue: node["v"].getFloat())
    of "Float2":
      result = (
          node["v"][0].getFloat().float64, 
          node["v"][1].getFloat().float64
        ).CBVar
    of "Float3":
      result = (
          node["v"][0].getFloat().float64, 
          node["v"][1].getFloat().float64, 
          node["v"][2].getFloat().float64
        ).CBVar
    of "Float4":
      result = (
          node["v"][0].getFloat().float64, 
          node["v"][1].getFloat().float64, 
          node["v"][2].getFloat().float64, 
          node["v"][3].getFloat().float64
        ).CBVar
    of "String":
      discard
      # result = CBVar(valueType: String, stringValue: node["v"].getStr())
    of "ContextVar":
      discard
      # result = CBVar(valueType: ContextVar, stringValue: node["v"].getStr())
    of "Color":
      result = (
          node["v"][0].getInt().uint8, 
          node["v"][1].getInt().uint8, 
          node["v"][2].getInt().uint8, 
          node["v"][3].getInt().uint8
        ).CBVar
    of "Image": # TODO/Figure
      result = CBVar(valueType: CBType.Image) # Hmm, maybe use ref for the data, but where to stick it?
    of "BoolOp":
      let ops = { Equal, More, Less, MoreEqual, LessEqual }
      for op in ops:
        if $op == node["v"].getStr():
          result = CBVar(valueType: BoolOp, boolOpValue: op)
          break
    of "Seq":
      discard
      # result = CBVar(valueType: Seq)
      # for subn in node["v"]:
      #   var subnCopy = subn
      #   result.seqValue.add(json2Var(subnCopy))
    of "Chain":
      discard
      # TODO - owned for new runtime
      # result = CBVar(valueType: Chain)
      # new result.chainValue
      # for subn in node["v"]:
      #   result.chainValue.blocks.add json2Block(subn)

proc toJson*(v: CBVar): string =
  let jstr = $var2Json(v)
  purgeJsonCache()
  return jstr

proc fromJson*(v: string): CBVar =
  var
    jnode = parseJson(v).toJsonNode()
    jvar = json2Var(jnode)
  purgeJsonCache()
  return jvar

proc block2Json(blk: ptr CBRuntimeBlock): JsonNode =
  var jnode = newJObject()
  jnode["name"] = %blk.name(blk).string

  var paramsNode: seq[JsonNode]
  let paramsLen = blk.parameters(blk).len
  for i in 0..<paramsLen:
    var
      param = blk.getParam(blk, i)
      paramJson = var2Json(param)
    paramsNode.add(paramJson)
    
  jnode["params"] = %paramsNode
  return jnode

proc json2Block(json: var JsonNode): ptr CBRuntimeBlock =
  result = createBlock(json["name"].getStr())
  result.setup(result)
  let paramsLen = result.parameters(result).len
  for i in 0..<paramsLen:
    var paramValue = json["params"][i]
    result.setParam(result, i, json2Var(paramValue))

when not defined(skipCoreBlocks):
  include blocks/internal/[core, stack, calculate]

# Swaps from compile time chain mode on/off
macro setCompileTimeChain*(enabled: bool) = compileTimeMode = enabled.boolVal

template compileTimeChain*(body: untyped): untyped =
  block:
    setCompileTimeChain(true)
    body
    setCompileTimeChain(false)

# Unrolls the setup and params settings of the current compile time chain once
macro prepareCompileTimeChain*(): untyped =
  result = newStmtList()

  for blkData in staticChainBlocks:
    var
      blk = blkData.blk
      params = blkData.params
    result.add quote do:
      `blk`.setup()
    for i in 0..params.high:
      var param = params[i]
      result.add quote do:
        `blk`.setParam(`i`, `param`)

# Unrolls the current compile time chain once
macro synthesizeCompileTimeChain*(input: untyped): untyped =
  result = newStmtList()
  var
    currentOutput = input
    ctx = gensym(nskVar)

  result.add quote do:
    var `ctx` = new CBContext

  for blkData in staticChainBlocks:
    var blk = blkData.blk
    result.add quote do:
      `currentOutput` = `blk`.activate(`ctx`, `currentOutput`)
  
  result.add quote do:
    `currentOutput`
  
# Clear the compile time chain state
macro clearCompileTimeChain*() = staticChainBlocks.setLen(0) # consume all

when isMainModule:
  import os
  
  type
    CBPow2Block = object
      inputValue: float
      params: array[1, CBVar]
  
  template inputTypes*(b: CBPow2Block): CBTypesInfo = { Float }
  template outputTypes*(b: CBPow2Block): CBTypesInfo = { Float }
  template parameters*(b: CBPow2Block): CBParametersInfo = @[("test", { Int })]
  template setParam*(b: CBPow2Block; index: int; val: CBVar) = b.params[0] = val
  template getParam*(b: CBPow2Block; index: int): CBVar = b.params[0]
  template activate*(b: CBPow2Block; context: CBContext; input: CBVar): CBVar = (input.floatValue * input.floatValue).CBVar

  chainblock CBPow2Block, "Pow2StaticBlock"

  proc testJson() =
    template jsonTestVar(x: CBVar): untyped =
      echo x
      var jvar = var2Json(x)
      echo jvar
      let varval = json2Var(jvar)
      echo varVal
      if x != varval:
        raiseAssert("Failed json test with var: " & $x.valueType)
    
    var types = { None, Bool, Int, Int2, Int3, Int4, Float, Float2, Float3, Float4, String, Color, CBType.Image, BoolOp, Seq,  Chain }
    for t in types:
      case t
      of Any, None, Object:
        jsonTestVar(Empty)
        jsonTestVar(RestartChain)
        jsonTestVar(StopChain)
        jsonTestVar(CBVar(valueType: Any))
      of Bool:
        jsonTestVar(true)
        jsonTestVar(false)
      of Int:
        jsonTestVar(int64.low)
        jsonTestVar(int64.high)
      of Int2:
        jsonTestVar((int64.low, int64.high))
        jsonTestVar((int64.high, int64.low))
      of Int3:
        jsonTestVar((int64.low, int64.high, int64.low))
        jsonTestVar((int64.high, int64.low, int64.high))
      of Int4:
        jsonTestVar((int64.low, int64.high, int64.low, int64.high))
        jsonTestVar((int64.high, int64.low, int64.high, int64.low))
      of Float:
        jsonTestVar(float64.low)
        jsonTestVar(float64.high)
      of Float2:
        jsonTestVar((float64.low, float64.high))
        jsonTestVar((float64.high, float64.low))
      of Float3:
        jsonTestVar((float64.low, float64.high, float64.low))
        jsonTestVar((float64.high, float64.low, float64.high))
      of Float4:
        jsonTestVar((float64.low, float64.high, float64.low, float64.high))
        jsonTestVar((float64.high, float64.low, float64.high, float64.low))
      of String, ContextVar:
        discard
        # jsonTestVar("Hello chainworld.")
        # jsonTestVar(~~"Hello chainworld.")
      of Color:
        jsonTestVar((1'u8, 2'u8, 3'u8, 255'u8))
      of BoolOp:
        jsonTestVar CBVar(valueType: BoolOp, boolOpValue: More)
        jsonTestVar CBVar(valueType: BoolOp, boolOpValue: Less)
      of Seq:
        discard
        # var vars = @[1.CBVar, 3.0.CBVar, "Hello".CBVar]
        # jsonTestVar CBVar(valueType: Seq, seqValue: vars)
      of Chain:
        discard
        # withChain jsonTestChain:
        #   Msg "This"
        #   Msg "Is A"
        #   Msg "Chain"
        #   Log()
        #   If:
        #     Operator: More
        #     Operand: 10
        # jsonTestVar(CBVar(valueType: Chain, chainValue: jsonTestChain))
      of CBType.Image:
        discard

  proc run() =
    var
      pblock1: CBPow2Block
    pblock1.setup()
    var res1 = pblock1.activate(nil, 2.0)
    echo res1.float
    
    var pblock2 = createBlock("Pow2StaticBlock")
    assert pblock2 != nil
    var param0Var = 10.CBVar
    echo param0Var.valueType
    pblock2.setParam(pblock2, 0, param0Var)
    echo pblock2.getParam(pblock2, 0).valueType
    var res2 = pblock2.activate(pblock2, nil, 2.0)
    echo res2.float
    echo pblock2.block2Json()
    var jt = pblock2.block2Json()

    var pblock3 = json2Block(jt)
    var res3 = pblock3.activate(pblock3, nil, 2.0)
    echo res3.float
    echo pblock3.block2Json

    # var
    #   ifblock = createBlock("If")
    
    # withChain trueMsg:
    #   Msg "True"
    
    # withChain falseMsg:
    #   Msg "False"
    
    # ifblock.setParam(ifblock, 0, More)
    # ifblock.setParam(ifblock, 1, 10)
    # ifblock.setParam(ifblock, 2, addr trueMsg)
    # ifblock.setParam(ifblock, 3, addr falseMsg)

    # echo ifblock.activate(ifblock, nil, 20)
    # echo ifblock.activate(ifblock, nil, 10)

    # echo ifblock.block2Json()
    # var newifblock = ifblock.block2Json().json2Block()

    # echo newifblock.activate(ctx, 20).valueType
    # echo newifblock.activate(ctx, 10).valueType

    # echo var2Json (10'u8, 20'u8, 30'u8, 255'u8)
    # var color = var2Json((10'u8, 20'u8, 30'u8, 255'u8)).json2Var()
    # echo var2Json color

    var
      mainChain = initChain()
    setCurrentChain(mainChain)

    withChain chain1:
      Msg "SubChain!"
      Sleep 0.2

    chain1.start(true)
    for i in 0..3:
      echo "Iteration ", i
      chain1.tick()
      sleep(500)
    discard chain1.stop()
    echo "Stopped"
    chain1.tick() # should do nothing
    echo "Done"

    var
      subChain1 = initChain()
    setCurrentChain(subChain1)
    
    Const "Hey hey"
    Log()
    
    setCurrentChain(mainChain)
    
    Log()
    Msg "Hello"
    Msg "World"
    Const 15
    If:
      Operator: MoreEqual
      Operand: 10
      True: addr subChain1
    Sleep 0.0
    Const 11
    ToString()
    Log()
    
    mainChain.start(true)
    for i in 0..10:
      var idx = i.CBVar
      mainChain.tick(idx)
    
    discard mainChain.stop()

    mainChain.start(true)
    for i in 0..10:
      var idx = i.CBVar
      mainChain.tick(idx)    

    # compileTimeChain:
    #   Msg "Hello"
    #   Msg "Static"
    #   Msg "World"
    
    # prepareCompileTimeChain()
    # discard synthesizeCompileTimeChain(Empty)

    # proc test1() =
    #   discard synthesizeCompileTimeChain(Empty)

    # proc test2() =
    #   discard synthesizeCompileTimeChain(Empty)

    # test1()
    # test2()

    # clearCompileTimeChain()

    # var cblk: CBlockConst
    # echo cblk.parameters()

    # reset()
  
  testJson()
  run()
  
  echo "Done"