# ChainBlocks
# Inspired by apple Shortcuts actually..
# Simply define a program by chaining blocks (black boxes), that have input and output/s

import sequtils, macros, strutils
import images

import types
export types

import logging
export logging

import nimline

when not defined cmake:
  when appType != "lib" or defined(forceCBRuntime):
    {.compile: "../core/runtime.cpp".}
    {.compile: "../core/3rdparty/easylogging++.cc".}

    cppdefines("ELPP_FEATURE_CRASH_LOG")
    
    when defined windows:
      {.passL: "-static -lboost_context-mt".}
      {.passC: "-static -DCHAINBLOCKS_RUNTIME".}
    else:
      {.passL: "-static -pthread -lboost_context".}
      {.passC: "-static -pthread -DCHAINBLOCKS_RUNTIME".}
  else:
    when defined windows:
      {.passL: "-static".}
      {.passC: "-static".}
    else:
      {.passL: "-static -pthread".}
      {.passC: "-static -pthread".}
    
    {.emit: """/*INCLUDESECTION*/
  #define STB_DS_IMPLEMENTATION 1
  """.}

{.passC: "-ffast-math".}

const
  FragCC* = 1718772071

proc elems*(v: CBInt2): array[2, int64] {.inline.} = [v.toCpp[0].to(int64), v.toCpp[1].to(int64)]
proc elems*(v: CBInt3): array[3, int32] {.inline.} = [v.toCpp[0].to(int32), v.toCpp[1].to(int32), v.toCpp[2].to(int32)]
proc elems*(v: CBInt4): array[4, int32] {.inline.} = [v.toCpp[0].to(int32), v.toCpp[1].to(int32), v.toCpp[2].to(int32), v.toCpp[3].to(int32)]
proc elems*(v: CBInt8): array[8, int16] {.inline.} = [v.toCpp[0].to(int16), v.toCpp[1].to(int16), v.toCpp[2].to(int16), v.toCpp[3].to(int16), v.toCpp[4].to(int16), v.toCpp[5].to(int16), v.toCpp[6].to(int16), v.toCpp[7].to(int16)]
proc elems*(v: CBInt16): array[16, int8] {.inline.} = 
  [ 
    v.toCpp[0].to(int8),  v.toCpp[1].to(int8),  v.toCpp[2].to(int8),  v.toCpp[3].to(int8), 
    v.toCpp[4].to(int8),  v.toCpp[5].to(int8),  v.toCpp[6].to(int8),  v.toCpp[7].to(int8),
    v.toCpp[8].to(int8),  v.toCpp[9].to(int8),  v.toCpp[10].to(int8), v.toCpp[11].to(int8),
    v.toCpp[12].to(int8), v.toCpp[13].to(int8), v.toCpp[14].to(int8), v.toCpp[15].to(int8)
  ]
proc elems*(v: CBFloat2): array[2, float64] {.inline.} = [v.toCpp[0].to(float64), v.toCpp[1].to(float64)]
proc elems*(v: CBFloat3): array[3, float32] {.inline.} = [v.toCpp[0].to(float32), v.toCpp[1].to(float32), v.toCpp[2].to(float32)]
proc elems*(v: CBFloat4): array[4, float32] {.inline.} = [v.toCpp[0].to(float32), v.toCpp[1].to(float32), v.toCpp[2].to(float32), v.toCpp[3].to(float32)]

converter toCBTypesInfo*(s: tuple[types: set[CBType]; canBeSeq: bool]): CBTypesInfo {.inline, noinit.} =
  initSeq(result)
  for t in s.types:
    global.stbds_arrpush(result, CBTypeInfo(basicType: t, sequenced: s.canBeSeq)).to(void)

converter toCBTypesInfo*(s: set[CBType]): CBTypesInfo {.inline, noinit.} = (s, false)

converter toCBTypeInfo*(s: tuple[ty: CBType, isSeq: bool]): CBTypeInfo {.inline, noinit.} =
  CBTypeInfo(basicType: s.ty, sequenced: s.isSeq)

converter toCBTypeInfo*(s: CBType): CBTypeInfo {.inline, noinit.} = (s, false)

converter toCBTypesInfo*(s: CBTypeInfo): CBTypesInfo {.inline, noinit.} =
  initSeq(result)
  result.push(s)

converter toCBTypesInfo*(s: StbSeq[CBTypeInfo]): CBTypesInfo {.inline, noinit.} =
  initSeq(result)
  for ti in s:
    result.push(ti)

converter toCBParameterInfo*(record: tuple[paramName: cstring; helpText: cstring; actualTypes: tuple[types: set[CBType]; canBeSeq: bool]]): CBParameterInfo {.inline.} =
  # This relies on const strings!! will crash if not!
  result.valueTypes = record.actualTypes.toCBTypesInfo()
  let
    pname = record.paramName
    help = record.helpText
  result.name = pname
  result.help = help

converter toCBParameterInfo*(record: tuple[paramName: cstring; helpText: cstring; actualTypes: CBTypesInfo]): CBParameterInfo {.inline.} =
  # This relies on const strings!! will crash if not!
  result.valueTypes = record.actualTypes
  let
    pname = record.paramName
    help = record.helpText
  result.name = pname
  result.help = help

converter toCBParameterInfo*(s: tuple[paramName: cstring; actualTypes: set[CBType]]): CBParameterInfo {.inline, noinit.} = (s.paramName, "".cstring, (s.actualTypes, false))
converter toCBParameterInfo*(s: tuple[paramName: cstring; helpText: cstring; actualTypes: set[CBType]]): CBParameterInfo {.inline, noinit.} = (s.paramName, s.helpText, (s.actualTypes, false))
converter toCBParameterInfo*(s: tuple[paramName: cstring; actualTypes: CBTypesInfo]): CBParameterInfo {.inline, noinit.} = (s.paramName, "".cstring, s.actualTypes)
converter toCBParameterInfo*(s: tuple[paramName: cstring; actualTypes: tuple[types: set[CBType]; canBeSeq: bool]]): CBParameterInfo {.inline, noinit.} = (s.paramName, "".cstring, (s.actualTypes.types, s.actualTypes.canBeSeq))

converter toCBParametersInfo*(s: tuple[paramName: cstring; actualTypes: set[CBType]]): CBParametersInfo {.inline, noinit.} = result.push((s.paramName, "".cstring, (s.actualTypes, false)).toCBParameterInfo())
converter toCBParametersInfo*(s: tuple[paramName: cstring; helpText: cstring; actualTypes: set[CBType]]): CBParametersInfo {.inline, noinit.} = result.push((s.paramName, s.helpText, (s.actualTypes, false)).toCBParameterInfo())
converter toCBParametersInfo*(s: tuple[paramName: cstring; helpText: cstring; actualTypes: tuple[types: set[CBType]; canBeSeq: bool]]): CBParametersInfo {.inline, noinit.} = result.push((s.paramName, s.helpText, (s.actualTypes.types, s.actualTypes.canBeSeq)).toCBParameterInfo())
converter toCBParametersInfo*(s: tuple[paramName: cstring; actualTypes: CBTypesInfo]): CBParametersInfo {.inline, noinit.} = result.push((s.paramName, "".cstring, s.actualTypes).toCBParameterInfo())
converter toCBParametersInfo*(s: tuple[paramName: cstring; actualTypes: tuple[types: set[CBType]; canBeSeq: bool]]): CBParametersInfo {.inline, noinit.} = result.push((s.paramName, "".cstring, (s.actualTypes.types, s.actualTypes.canBeSeq)).toCBParameterInfo())

converter cbParamsToSeq*(params: var CBParametersInfo): StbSeq[CBParameterInfo] =
  for item in params.items:
    result.push(item)

converter toCBTypesInfoTuple*(s: tuple[a: cstring; b: set[CBType]]): tuple[c: cstring; d: CBTypesInfo] {.inline, noinit.} =
  result[0] = s.a
  result[1] = s.b

converter toCBParametersInfo*(s: StbSeq[tuple[paramName: cstring; helpText: cstring; actualTypes: tuple[types: set[CBType]; canBeSeq: bool]]]): CBParametersInfo {.inline, noinit.} =
  initSeq(result)
  for i in 0..s.high:
    var record = s[i]
    result.push(record.toCBParameterInfo())

converter toCBParametersInfo*(s: StbSeq[tuple[paramName: cstring; actualTypes: set[CBType]]]): CBParametersInfo {.inline, noinit.} =
  initSeq(result)
  for i in 0..s.high:
    var record = s[i]
    result.push(record.toCBParameterInfo())

converter toCBParametersInfo*(s: StbSeq[tuple[paramName: cstring; helpText: cstring; actualTypes: set[CBType]]]): CBParametersInfo {.inline.} =
  initSeq(result)
  for i in 0..s.high:
    var record = s[i]
    result.push(record.toCBParameterInfo())

converter toCBParametersInfo*(s: StbSeq[tuple[paramName: cstring; actualTypes: CBTypesInfo]]): CBParametersInfo {.inline.} =
  initSeq(result)
  for i in 0..s.high:
    var record = s[i]
    result.push(record.toCBParameterInfo())

converter toCBParametersInfo*(s: StbSeq[tuple[paramName: cstring; actualTypes: tuple[types: set[CBType]; canBeSeq: bool]]]): CBParametersInfo {.inline.} =
  initSeq(result)
  for i in 0..s.high:
    var record = s[i]
    result.push(record.toCBParameterInfo())

converter toCBParametersInfo*(s: StbSeq[tuple[paramName: cstring; actualTypes: StbSeq[CBTypeInfo]]]): CBParametersInfo {.inline.} =
  initSeq(result)
  for i in 0..s.high:
    var
      record = s[i]
      pinfo = CBParameterInfo(name: record.paramName)
    initSeq(pinfo.valueTypes)
    for at in record.actualTypes.mitems:
      pinfo.valueTypes.push(at)
    result.push(pinfo)

template iterateTuple(t: tuple; storage: untyped; castType: untyped): untyped =
  var idx = 0
  for x in t.fields:
    storage.toCpp[idx] = x.castType
    inc idx

converter toInt64Value*(v: CBVar): int64 {.inline.} =
  assert v.valueType == Int
  return v.payload.intValue

converter toIntValue*(v: CBVar): int {.inline.} =
  assert v.valueType == Int
  return v.payload.intValue.int

converter toFloatValue*(v: CBVar): float64 {.inline.} =
  assert v.valueType == Float
  return v.payload.floatValue

converter toBoolValue*(v: CBVar): bool {.inline.} =
  assert v.valueType == Bool
  return v.payload.boolValue

converter toCBVar*(cbSeq: CBSeq): CBVar {.inline.} =
  result.valueType = Seq
  result.payload.seqValue = cbSeq
  result.payload.seqLen = -1

converter toCBVar*(cbStr: CBString): CBVar {.inline.} =
  result.valueType = String
  result.payload.stringValue = cbStr

converter imageTypesConv*(cbImg: CBImage): Image[uint8] {.inline, noinit.} =
  result.width = cbImg.width.int
  result.height = cbImg.height.int
  result.channels = cbImg.channels.int
  result.data = cbImg.data

converter imageTypesConv*(cbImg: Image[uint8]): CBImage {.inline, noinit.} =
  result.width = cbImg.width.uint16
  result.height = cbImg.height.uint16
  result.channels = cbImg.channels.uint8
  result.data = cbImg.data

converter toCString*(cbStr: CBString): cstring {.inline, noinit.} = cast[cstring](cbStr)

converter toCBVar*(v: CBImage): CBVar {.inline.} =
  return CBVar(valueType: CBType.Image, payload: CBVarPayload(imageValue: v))

proc asCBVar*(v: pointer; vendorId, typeId: int32): CBVar {.inline.} =
  return CBVar(valueType: Object, payload: CBVarPayload(objectValue: v, objectVendorId: vendorId, objectTypeId: typeId))

converter toCBVar*(v: int): CBVar {.inline.} =
  return CBVar(valueType: Int, payload: CBVarPayload(intValue: v))

converter toCBVar*(v: int64): CBVar {.inline.} =
  return CBVar(valueType: Int, payload: CBVarPayload(intValue: v))

converter toCBVar*(v: CBInt2): CBVar {.inline.} =
  return CBVar(valueType: Int2, payload: CBVarPayload(int2Value: v))

converter toCBVar*(v: CBInt3): CBVar {.inline.} =
  return CBVar(valueType: Int3, payload: CBVarPayload(int3Value: v))

converter toCBVar*(v: CBInt4): CBVar {.inline.} =
  return CBVar(valueType: Int4, payload: CBVarPayload(int4Value: v))

converter toCBVar*(v: CBInt8): CBVar {.inline.} =
  return CBVar(valueType: Int8, payload: CBVarPayload(int8Value: v))

converter toCBVar*(v: CBInt16): CBVar {.inline.} =
  return CBVar(valueType: Int16, payload: CBVarPayload(int16Value: v))

converter toCBVar*(v: tuple[a,b: int]): CBVar {.inline.} =
  result.valueType = Int2
  iterateTuple(v, result.payload.int2Value, int64)

converter toCBVar*(v: tuple[a,b: int64]): CBVar {.inline.} =
  result.valueType = Int2
  iterateTuple(v, result.payload.int2Value, int64)

converter toCBVar*(v: tuple[a,b,c: int]): CBVar {.inline.} =
  result.valueType = Int3
  iterateTuple(v, result.payload.int3Value, int32)

converter toCBVar*(v: tuple[a,b,c: int32]): CBVar {.inline.} =
  result.valueType = Int3
  iterateTuple(v, result.payload.int3Value, int32)

converter toCBVar*(v: tuple[a,b,c,d: int]): CBVar {.inline.} =
  result.valueType = Int4
  iterateTuple(v, result.payload.int4Value, int32)

converter toCBVar*(v: tuple[a,b,c,d: int32]): CBVar {.inline.} =
  result.valueType = Int4
  iterateTuple(v, result.payload.int4Value, int32)

converter toCBVar*(v: tuple[a, b, c, d, e, f, g, h: int]): CBVar {.inline.} =
  result.valueType = Int8
  iterateTuple(v, result.payload.int8Value, int16)

converter toCBVar*(v: tuple[a, b, c, d, e, f, g, h: int16]): CBVar {.inline.} =
  result.valueType = Int8
  iterateTuple(v, result.payload.int8Value, int16)

converter toCBVar*(v: tuple[a, b, c, d, e, f, g, h, a1, b1, c1, d1, e1, f1, g1, h1: int]): CBVar {.inline.} =
  result.valueType = Int16
  iterateTuple(v, result.payload.int16Value, int8)

converter toCBVar*(v: tuple[a, b, c, d, e, f, g, h, a1, b1, c1, d1, e1, f1, g1, h1: int8]): CBVar {.inline.} =
  result.valueType = Int16
  iterateTuple(v, result.payload.int16Value, int8)

converter toCBVar*(v: bool): CBVar {.inline.} =
  return CBVar(valueType: Bool, payload: CBVarPayload(boolValue: v))

converter toCBVar*(v: float32): CBVar {.inline.} =
  return CBVar(valueType: Float, payload: CBVarPayload(floatValue: v))

converter toCBVar*(v: float64): CBVar {.inline.} =
  return CBVar(valueType: Float, payload: CBVarPayload(floatValue: v))

converter toCBVar*(v: CBFloat2): CBVar {.inline.} =
  return CBVar(valueType: Float2, payload: CBVarPayload(float2Value: v))

converter toCBVar*(v: CBFloat3): CBVar {.inline.} =
  return CBVar(valueType: Float3, payload: CBVarPayload(float3Value: v))

converter toCBVar*(v: CBFloat4): CBVar {.inline.} =
  return CBVar(valueType: Float4, payload: CBVarPayload(float4Value: v))

converter toCBVar*(v: tuple[a,b: float64]): CBVar {.inline.} =
  result.valueType = Float2
  iterateTuple(v, result.payload.float2Value, float64)

converter toCBVar*(v: tuple[a,b,c: float]): CBVar {.inline.} =
  result.valueType = Float3
  iterateTuple(v, result.payload.float3Value, float32)

converter toCBVar*(v: tuple[a,b,c: float32]): CBVar {.inline.} =
  result.valueType = Float3
  iterateTuple(v, result.payload.float3Value, float32)

converter toCBVar*(v: tuple[a,b,c,d: float]): CBVar {.inline.} =
  result.valueType = Float4
  iterateTuple(v, result.payload.float4Value, float32)

converter toCBVar*(v: tuple[a,b,c,d: float32]): CBVar {.inline.} =
  result.valueType = Float4
  iterateTuple(v, result.payload.float4Value, float32)

converter toCBVar*(v: tuple[r,g,b,a: uint8]): CBVar {.inline.} =
  return CBVar(valueType: Color, payload: CBVarPayload(colorValue: CBColor(r: v.r, g: v.g, b: v.b, a: v.a)))

converter toCBVar*(v: CBColor): CBVar {.inline.} =
  return CBVar(valueType: Color, payload: CBVarPayload(colorValue: v))

converter toCBVar*(v: CBChainPtr): CBVar {.inline.} =
  return CBVar(valueType: Chain, payload: CBVarPayload(chainValue: v))

converter toCBVar*(v: ptr CBRuntimeBlock): CBVar {.inline.} =
  return CBVar(valueType: Block, payload: CBVarPayload(blockValue: v))

template contextOrPure*(subject, container: untyped; wantedType: CBType; typeGetter: untyped): untyped =
  if container.valueType == ContextVar:
    var foundVar = context.contextVariable(container.payload.stringValue)
    if foundVar[].valueType == wantedType:
      subject = foundVar[].typeGetter
  else:
    subject = container.typeGetter

proc contextVariable*(name: string): CBVar {.inline.} =
  return CBVar(valueType: ContextVar, payload: CBVarPayload(stringValue: name.cstring.CBString))

template `~~`*(name: string): CBVar = contextVariable(name)

template withChain*(chain, body: untyped): untyped =
  var `chain` {.inject.} = newChain(astToStr(`chain`))
  var prev = getCurrentChain()
  setCurrentChain(chain)
  body
  setCurrentChain(prev)

let noParams: CBParametersInfo = nil
let noExpose: CBExposedTypesInfo = nil

# Make those optional
template help*(b: auto): cstring = ""
template setup*(b: auto) = discard
template destroy*(b: auto) = discard
template exposedVariables*(b: auto): CBExposedTypesInfo = noExpose
template consumedVariables*(b: auto): CBExposedTypesInfo = noExpose
template parameters*(b: auto): CBParametersInfo = noParams
template setParam*(b: auto; index: int; val: CBVar) = discard
template getParam*(b: auto; index: int): CBVar = CBVar(valueType: None)
template cleanup*(b: auto) = discard

when appType != "lib" or defined(forceCBRuntime):
  proc throwCBException*(msg: string) = emitc("throw chainblocks::CBException(", msg.cstring, ");")

  proc createBlock*(name: cstring): ptr CBRuntimeBlock {.importcpp: "chainblocks::createBlock(#)", header: "runtime.hpp".}
  proc cbCreateBlock*(name: cstring): ptr CBRuntimeBlock {.cdecl, exportc, dynlib.} = createBlock(name)

  proc getCurrentChain*(): CBChainPtr {.importcpp: "chainblocks::getCurrentChain()", header: "runtime.hpp".}
  proc cbGetCurrentChain*(): CBChainPtr {.cdecl, exportc, dynlib.} = getCurrentChain()

  proc setCurrentChain*(chain: CBChainPtr) {.importcpp: "chainblocks::setCurrentChain(#)", header: "runtime.hpp".}
  proc cbSetCurrentChain*(chain: CBChainPtr) {.cdecl, exportc, dynlib.} = setCurrentChain(chain)

  proc globalVariable*(name: cstring): ptr CBVar {.importcpp: "chainblocks::globalVariable(#)", header: "runtime.hpp".}
  proc hasGlobalVariable*(name: cstring): bool {.importcpp: "chainblocks::hasGlobalVariable(#)", header: "runtime.hpp".}
  
  type 
    CBValidationCallback* {.importcpp: "CBValidationCallback", header: "runtime.hpp".} = proc(blk: ptr CBRuntimeBlock; error: cstring; nonfatalWarning: bool; userData: pointer) {.cdecl.}
    ValidationResults* = seq[tuple[error: bool; message: string]]
  proc validateConnections*(chain: CBChainPtr; callback: CBValidationCallback; userData: pointer; inputType: CBTypeInfo = None): CBTypeInfo {.importcpp: "validateConnections(#, #, #, #)", header: "runtime.hpp".}
  proc validateConnections*(chain: CBRuntimeBlocks; callback: CBValidationCallback; userData: pointer; inputType: CBTypeInfo = None): CBTypeInfo {.importcpp: "validateConnections(#, #, #, #)", header: "runtime.hpp".}
  proc validateSetParam*(blk: ptr CBRuntimeBlock; index: cint;  value: var CBVar; callback: CBValidationCallback; userData: pointer) {.importcpp: "validateSetParam(#, #, #, #, #)", header: "runtime.hpp".}
  
  proc validate*(chain: CBChainPtr): ValidationResults =
    discard validateConnections(
      chain,
      proc(blk: ptr CBRuntimeBlock; error: cstring; nonfatalWarning: bool; userData: pointer) {.cdecl.} =
        var resp = cast[ptr ValidationResults](userData)
        resp[].add((not nonfatalWarning, $error)),
      addr result
    )
  
  proc validate*(blk: ptr CBRuntimeBlock; index: int;  value: var CBVar): ValidationResults =
    validateSetParam(
      blk,
      index.cint,
      value,
      proc(blk: ptr CBRuntimeBlock; error: cstring; nonfatalWarning: bool; userData: pointer) {.cdecl.} =
        var resp = cast[ptr ValidationResults](userData)
        resp[].add((not nonfatalWarning, $error)),
      addr result
    )

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

macro chainblock*(blk: untyped; blockName: string; namespaceStr: string = ""; testCode: untyped = nil): untyped =
  var
    rtName = ident($blk & "RT")
    rtNameValue = ident($blk & "RTValue")
    macroName {.used.} = ident($blockName)
    namespace = if $namespaceStr != "": $namespaceStr & "." else: ""
    testName {.used.} = ident("test_" & $blk)

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
    
    inferTypesProc = ident($blk & "_inferTypes")
    
    activateProc = ident($blk & "_activate")
    cleanupProc = ident($blk & "_cleanup")
  
  result = quote do:
    import macros # used!
    
    type
      `rtNameValue` = object
        pre: CBRuntimeBlock
        sb: `blk`
        cacheInputTypes: owned(ref CBTypesInfo)
        cacheOutputTypes: owned(ref CBTypesInfo)
        cacheExposedVariables: owned(ref CBExposedTypesInfo)
        cacheConsumedVariables: owned(ref CBExposedTypesInfo)
        cacheParameters: owned(ref CBParametersInfo)
      
      `rtName`* = ptr `rtNameValue`
    
    template name*(b: `blk`): string =
      (`namespace` & `blockName`)
    proc `nameProc`*(b: `rtName`): cstring {.cdecl.} =
      updateStackBottom()
      (`namespace` & `blockName`)
    proc `helpProc`*(b: `rtName`): cstring {.cdecl.} =
      updateStackBottom()
      b.sb.help()
    proc `setupProc`*(b: `rtName`) {.cdecl.} =
      updateStackBottom()
      b.sb.setup()
    proc `destroyProc`*(b: `rtName`) {.cdecl.} =
      updateStackBottom()
      b.sb.destroy()
      if b.cacheInputTypes != nil:
        freeSeq(b.cacheInputTypes[])
        b.cacheInputTypes = nil
      if b.cacheOutputTypes != nil:
        freeSeq(b.cacheOutputTypes[])
        b.cacheOutputTypes = nil
      if b.cacheExposedVariables != nil:
        freeSeq(b.cacheExposedVariables[])
        b.cacheExposedVariables = nil
      if b.cacheConsumedVariables != nil:
        freeSeq(b.cacheConsumedVariables[])
        b.cacheConsumedVariables = nil
      if b.cacheParameters != nil:
        for item in b.cacheParameters[].mitems:
          freeSeq(item.valueTypes)
        freeSeq(b.cacheParameters[])
        b.cacheParameters = nil
      cppdel(b)
    when compiles((var x: `blk`; x.preChain(nil))):
      proc `preChainProc`*(b: `rtName`, context: CBContext) {.cdecl.} =
        updateStackBottom()
        b.sb.preChain(context)
    when compiles((var x: `blk`; x.postChain(nil))):
      proc `postChainProc`*(b: `rtName`, context: CBContext) {.cdecl.} =
        updateStackBottom()
        b.sb.postChain(context)
    proc `inputTypesProc`*(b: `rtName`): CBTypesInfo {.cdecl.} =
      updateStackBottom()
      if b.cacheInputTypes == nil:
        new b.cacheInputTypes
        b.cacheInputTypes[] = b.sb.inputTypes()
      b.cacheInputTypes[]
    proc `outputTypesProc`*(b: `rtName`): CBTypesInfo {.cdecl.} =
      updateStackBottom()
      if b.cacheOutputTypes == nil:
        new b.cacheOutputTypes
        b.cacheOutputTypes[] = b.sb.outputTypes()
      b.cacheOutputTypes[]
    proc `exposedVariablesProc`*(b: `rtName`): CBExposedTypesInfo {.cdecl.} =
      updateStackBottom()
      if b.cacheExposedVariables == nil:
        new b.cacheExposedVariables
        b.cacheExposedVariables[] = b.sb.exposedVariables()
      b.cacheExposedVariables[]
    proc `consumedVariablesProc`*(b: `rtName`): CBExposedTypesInfo {.cdecl.} =
      updateStackBottom()
      if b.cacheConsumedVariables == nil:
        new b.cacheConsumedVariables
        b.cacheConsumedVariables[] = b.sb.consumedVariables()
      b.cacheConsumedVariables[]
    proc `parametersProc`*(b: `rtName`): CBParametersInfo {.cdecl.} =
      updateStackBottom()
      if b.cacheParameters == nil:
        new b.cacheParameters
        b.cacheParameters[] = b.sb.parameters()
      b.cacheParameters[]
    proc `setParamProc`*(b: `rtName`; index: int; val: CBVar) {.cdecl.} =
      updateStackBottom()
      b.sb.setParam(index, val)
    proc `getParamProc`*(b: `rtName`; index: int): CBVar {.cdecl.} =
      updateStackBottom()
      b.sb.getParam(index)
    when compiles((var x: `blk`; discard x.inferTypes(CBTypeInfo(), nil))):
      static:
        echo `blk`, " has inferTypes"
      proc `inferTypesProc`*(b: `rtName`; inputType: CBTypeInfo; consumables: CBExposedTypesInfo): CBTypeInfo {.cdecl.} =
        updateStackBottom()
        b.sb.inferTypes(inputType, consumables)
    proc `activateProc`*(b: `rtName`; context: CBContext; input: CBVar): CBVar {.cdecl.} =
      updateStackBottom()
      try:
        b.sb.activate(context, input)
      except:
        context.setError(getCurrentExceptionMsg())
        raise
    proc `cleanupProc`*(b: `rtName`) {.cdecl.} =
      updateStackBottom()
      b.sb.cleanup()
    registerBlock(`namespace` & `blockName`) do -> ptr CBRuntimeBlock {.cdecl.}:
      # https://stackoverflow.com/questions/7546620/operator-new-initializes-memory-to-zero
      # Memory will be memset to 0x0, because we call T()
      cppnew(result, CBRuntimeBlock, `rtNameValue`)
      # DO NOT CHANGE THE FOLLOWING, this sorcery is needed to build with msvc 19ish
      # Moreover it's kinda nim's fault, as it won't generate a C cast without `.pointer`
      result.name = cast[CBNameProc](`nameProc`.pointer)
      result.help = cast[CBHelpProc](`helpProc`.pointer)
      result.setup = cast[CBSetupProc](`setupProc`.pointer)
      result.destroy = cast[CBDestroyProc](`destroyProc`.pointer)
      
      # pre post are optional!
      when compiles((var x: `blk`; x.preChain(nil))):
        result.preChain = cast[CBPreChainProc](`preChainProc`.pointer)
      when compiles((var x: `blk`; x.postChain(nil))):
        result.postChain = cast[CBPostChainProc](`postChainProc`.pointer)
      
      result.inputTypes = cast[CBInputTypesProc](`inputTypesProc`.pointer)
      result.outputTypes = cast[CBOutputTypesProc](`outputTypesProc`.pointer)
      result.exposedVariables = cast[CBExposedVariablesProc](`exposedVariablesProc`.pointer)
      result.consumedVariables = cast[CBConsumedVariablesProc](`consumedVariablesProc`.pointer)
      result.parameters = cast[CBParametersProc](`parametersProc`.pointer)
      result.setParam = cast[CBSetParamProc](`setParamProc`.pointer)
      result.getParam = cast[CBGetParamProc](`getParamProc`.pointer)
      
      when compiles((var x: `blk`; discard x.inferTypes(CBTypeInfo(), nil))):
        result.inferTypes = cast[CBInferTypesProc](`inferTypesProc`.pointer)
      
      result.activate = cast[CBActivateProc](`activateProc`.pointer)
      result.cleanup = cast[CBCleanupProc](`cleanupProc`.pointer)

when appType != "lib" or defined(forceCBRuntime):
  # When building the runtime!
  type
    RunChainOutputState* {.importcpp: "chainblocks::RunChainOutputState", header: "runtime.hpp".} = enum
      Running, Restarted, Stopped, Failed
    
    RunChainOutput* {.importcpp: "chainblocks::RunChainOutput", header: "runtime.hpp".} = object
      output*: CBVar
      state*: RunChainOutputState

  proc registerBlock*(name: string; initProc: CBBlockConstructor) =
    invokeFunction("chainblocks::registerBlock", name, initProc).to(void)

  proc registerObjectType*(vendorId, typeId: int32; info: CBObjectInfo) =
    invokeFunction("chainblocks::registerObjectType", vendorId, typeId, info).to(void)

  proc registerEnumType*(vendorId, typeId: int32; info: CBEnumInfo) =
    invokeFunction("chainblocks::registerEnumType", vendorId, typeId, info).to(void)

  proc registerRunLoopCallback*(name: string; callback: CBCallback) =
    invokeFunction("chainblocks::registerRunLoopCallback", name, callback).to(void)

  proc unregisterRunLoopCallback*(name: string) =
    invokeFunction("chainblocks::unregisterRunLoopCallback", name).to(void)

  proc registerExitCallback*(name: string; callback: CBCallback) =
    invokeFunction("chainblocks::registerExitCallback", name, callback).to(void)

  proc unregisterExitCallback*(name: string) =
    invokeFunction("chainblocks::unregisterExitCallback", name).to(void)

  proc callExitCallbacks*() =
    invokeFunction("chainblocks::callExitCallbacks").to(void)

  proc contextVariable*(ctx: CBContext; name: cstring): ptr CBVar {.importcpp: "chainblocks::contextVariable(#, #)", header: "runtime.hpp".}

  proc setError*(ctx: CBContext; errorTxt: cstring) {.importcpp: "#->setError(#)", header: "runtime.hpp".}

  proc runChain*(chain: CBChainPtr, context: ptr CBContextObj; chainInput: CBVar): RunChainOutput {.importcpp: "chainblocks::runChain(#, #, #)", header: "runtime.hpp".}

  proc activateBlock*(chain: ptr CBRuntimeBlock, context: ptr CBContextObj; input: var CBVar; output: var CBVar) {.importcpp: "chainblocks::activateBlock(#, #, #, #)", header: "runtime.hpp".}
  
  proc suspendInternal(seconds: float64): CBVar {.importcpp: "chainblocks::suspend(#)", header: "runtime.hpp".}
  proc suspend*(seconds: float64): CBVar {.inline.} =
    var frame = getFrameState()
    result = suspendInternal(seconds)
    setFrameState(frame)

  proc isRunning*(chain: CBChainPtr): bool {.importcpp: "chainblocks::isRunning(#)", header: "runtime.hpp".}

  proc sleep*(secs: float64) {.importcpp: "chainblocks::sleep(#)", header: "runtime.hpp".}
  proc cbSleep*(secs: float64) {.cdecl, exportc, dynlib.} = sleep(secs)

  proc canceled*(ctx: CBContext): bool {.inline.} = ctx.aborted

# This template is inteded to be used inside blocks activations
template pause*(secs: float): untyped =
  let suspendRes = suspend(secs.float64)
  if suspendRes.chainState != Continue:
    return suspendRes

template halt*(context: CBContext; txt: string): untyped =
  context.setError(txt)
  StopChain

defineCppType(StdRegex, "std::regex", "<regex>")
defineCppType(StdSmatch, "std::smatch", "<regex>")
defineCppType(StdSSubMatch, "std::ssub_match", "<regex>")

include ops

when not defined(skipCoreBlocks):
  import os
  import nimline
  include blocks/internal/[core, strings, calculate, ipc]

# always try this for safety
assert sizeof(CBVar) == 32
assert sizeof(CBVarPayload) == 16
