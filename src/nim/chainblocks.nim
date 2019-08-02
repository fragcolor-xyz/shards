# ChainBlocks
# Inspired by apple Shortcuts actually..
# Simply define a program by chaining blocks (black boxes), that have input and output/s

import sequtils, macros, strutils
import fragments/[serialization]
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
  FragCC* = toFourCC("frag")

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

converter toCBParameterInfo*(record: tuple[paramName: cstring; helpText: cstring; actualTypes: tuple[types: set[CBType]; canBeSeq: bool]; usesContext: bool]): CBParameterInfo {.inline.} =
  # This relies on const strings!! will crash if not!
  result.valueTypes = record.actualTypes.toCBTypesInfo()
  let
    pname = record.paramName
    help = record.helpText
  result.name = pname
  result.help = help
  result.allowContext = record.usesContext

converter toCBParameterInfo*(record: tuple[paramName: cstring; helpText: cstring; actualTypes: CBTypesInfo; usesContext: bool]): CBParameterInfo {.inline.} =
  # This relies on const strings!! will crash if not!
  result.valueTypes = record.actualTypes
  let
    pname = record.paramName
    help = record.helpText
  result.name = pname
  result.help = help
  result.allowContext = record.usesContext

converter toCBParameterInfo*(s: tuple[paramName: cstring; actualTypes: set[CBType]]): CBParameterInfo {.inline, noinit.} = (s.paramName, "".cstring, (s.actualTypes, false), false)
converter toCBParameterInfo*(s: tuple[paramName: cstring; helpText: cstring; actualTypes: set[CBType]]): CBParameterInfo {.inline, noinit.} = (s.paramName, s.helpText, (s.actualTypes, false), false)
converter toCBParameterInfo*(s: tuple[paramName: cstring; helpText: cstring; actualTypes: tuple[types: set[CBType]; canBeSeq: bool]]): CBParameterInfo {.inline, noinit.} = (s.paramName, s.helpText, (s.actualTypes.types, s.actualTypes.canBeSeq), false)
converter toCBParameterInfo*(s: tuple[paramName: cstring; helpText: cstring; actualTypes: set[CBType]; usesContext: bool]): CBParameterInfo {.inline, noinit.} = (s.paramName, s.helpText, (s.actualTypes, false), s.usesContext)
converter toCBParameterInfo*(s: tuple[paramName: cstring; actualTypes: set[CBType], usesContext: bool]): CBParameterInfo {.inline, noinit.} = (s.paramName, "".cstring, (s.actualTypes, false), s.usesContext)
converter toCBParameterInfo*(s: tuple[paramName: cstring; actualTypes: CBTypesInfo, usesContext: bool]): CBParameterInfo {.inline, noinit.} = (s.paramName, "".cstring, s.actualTypes, s.usesContext)
converter toCBParameterInfo*(s: tuple[paramName: cstring; actualTypes: CBTypesInfo]): CBParameterInfo {.inline, noinit.} = (s.paramName, "".cstring, s.actualTypes, false)
converter toCBParameterInfo*(s: tuple[paramName: cstring; actualTypes: tuple[types: set[CBType]; canBeSeq: bool]]): CBParameterInfo {.inline, noinit.} = (s.paramName, "".cstring, (s.actualTypes.types, s.actualTypes.canBeSeq), false)
converter toCBParameterInfo*(s: tuple[paramName: cstring; actualTypes: tuple[types: set[CBType]; canBeSeq: bool]; usesContext: bool]): CBParameterInfo {.inline, noinit.} = (s.paramName, "".cstring, (s.actualTypes.types, s.actualTypes.canBeSeq), s.usesContext)

converter toCBParametersInfo*(s: tuple[paramName: cstring; actualTypes: set[CBType]]): CBParametersInfo {.inline, noinit.} = result.push((s.paramName, "".cstring, (s.actualTypes, false), false).toCBParameterInfo())
converter toCBParametersInfo*(s: tuple[paramName: cstring; helpText: cstring; actualTypes: set[CBType]]): CBParametersInfo {.inline, noinit.} = result.push((s.paramName, s.helpText, (s.actualTypes, false), false).toCBParameterInfo())
converter toCBParametersInfo*(s: tuple[paramName: cstring; helpText: cstring; actualTypes: tuple[types: set[CBType]; canBeSeq: bool]]): CBParametersInfo {.inline, noinit.} = result.push((s.paramName, s.helpText, (s.actualTypes.types, s.actualTypes.canBeSeq), false).toCBParameterInfo())
converter toCBParametersInfo*(s: tuple[paramName: cstring; helpText: cstring; actualTypes: set[CBType]; usesContext: bool]): CBParametersInfo {.inline, noinit.} = result.push((s.paramName, s.helpText, (s.actualTypes, false), s.usesContext).toCBParameterInfo())
converter toCBParametersInfo*(s: tuple[paramName: cstring; actualTypes: set[CBType], usesContext: bool]): CBParametersInfo {.inline, noinit.} = result.push((s.paramName, "".cstring, (s.actualTypes, false), s.usesContext).toCBParameterInfo())
converter toCBParametersInfo*(s: tuple[paramName: cstring; actualTypes: CBTypesInfo, usesContext: bool]): CBParametersInfo {.inline, noinit.} = result.push((s.paramName, "".cstring, s.actualTypes, s.usesContext).toCBParameterInfo())
converter toCBParametersInfo*(s: tuple[paramName: cstring; actualTypes: CBTypesInfo]): CBParametersInfo {.inline, noinit.} = result.push((s.paramName, "".cstring, s.actualTypes, false).toCBParameterInfo())
converter toCBParametersInfo*(s: tuple[paramName: cstring; actualTypes: tuple[types: set[CBType]; canBeSeq: bool]]): CBParametersInfo {.inline, noinit.} = result.push((s.paramName, "".cstring, (s.actualTypes.types, s.actualTypes.canBeSeq), false).toCBParameterInfo())
converter toCBParametersInfo*(s: tuple[paramName: cstring; actualTypes: tuple[types: set[CBType]; canBeSeq: bool]; usesContext: bool]): CBParametersInfo {.inline, noinit.} = result.push((s.paramName, "".cstring, (s.actualTypes.types, s.actualTypes.canBeSeq), s.usesContext).toCBParameterInfo())

converter cbParamsToSeq*(params: var CBParametersInfo): StbSeq[CBParameterInfo] =
  for item in params.items:
    result.push(item)

converter toCBTypesInfoTuple*(s: tuple[a: cstring; b: set[CBType]]): tuple[c: cstring; d: CBTypesInfo] {.inline, noinit.} =
  result[0] = s.a
  result[1] = s.b

converter toCBParametersInfo*(s: StbSeq[tuple[paramName: cstring; helpText: cstring; actualTypes: tuple[types: set[CBType]; canBeSeq: bool]; usesContext: bool]]): CBParametersInfo {.inline.} =
  initSeq(result)
  for i in 0..s.high:
    var record = s[i]
    result.push(record.toCBParameterInfo())

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

converter toCBParametersInfo*(s: StbSeq[tuple[paramName: cstring; actualTypes: set[CBType]; usesContext: bool]]): CBParametersInfo {.inline.} =
  initSeq(result)
  for i in 0..s.high:
    var record = s[i]
    result.push(record.toCBParameterInfo())

converter toCBParametersInfo*(s: StbSeq[tuple[paramName: cstring; actualTypes: CBTypesInfo; usesContext: bool]]): CBParametersInfo {.inline.} =
  initSeq(result)
  for i in 0..s.high:
    var record = s[i]
    result.push(record.toCBParameterInfo())

converter toCBParametersInfo*(s: StbSeq[tuple[paramName: cstring; helpText: cstring; actualTypes: set[CBType]; usesContext: bool]]): CBParametersInfo {.inline.} =
  initSeq(result)
  for i in 0..s.high:
    var record = s[i]
    result.push(record.toCBParameterInfo())

converter toCBParametersInfo*(s: StbSeq[tuple[paramName: cstring; actualTypes: tuple[types: set[CBType]; canBeSeq: bool]]]): CBParametersInfo {.inline.} =
  initSeq(result)
  for i in 0..s.high:
    var record = s[i]
    result.push(record.toCBParameterInfo())

converter toCBParametersInfo*(s: StbSeq[tuple[paramName: cstring; actualTypes: tuple[types: set[CBType]; canBeSeq: bool]; usesContext: bool]]): CBParametersInfo {.inline.} =
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

# template ti*(proxy: untyped): CBTypesInfo = proxy.toCBTypesInfo()
# template pi*(proxy: untyped): CBParametersInfo = proxy.toCBParametersInfo()

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
    pi1 = (cs"Param1", { Int, Int4 }).toCBParameterInfo()
  assert pi1.name == "Param1"
  assert pi1.valueTypes[0].basicType == Int
  assert pi1.valueTypes[1].basicType == Int4
  assert not pi1.allowContext

  var
    pi2 = (cs"Param2", { Int, Int4 }, true).toCBParameterInfo()
  assert pi2.name == "Param2"
  assert pi2.valueTypes[0].basicType == Int
  assert pi2.valueTypes[1].basicType == Int4
  assert pi2.allowContext

  var
    pis1 = toCBParametersInfo(*@[
      (cs"Key", { Int }),
      (cs"Ctrl", { Bool }),
      (cs"Alt", { Bool }),
      (cs"Shift", { Bool }),
    ])
  assert pis1[0].name == "Key"
  assert pis1[0].valueTypes[0].basicType == Int
  assert pis1[1].name == "Ctrl"
  assert pis1[1].valueTypes[0].basicType == Bool

  var
    pis2 = toCBParametersInfo(*@[(cs"Window", cs"A window name or variable if we wish to send the event only to a target window.", { String })])
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

proc asCBVar*(v: pointer; vendorId, typeId: FourCC): CBVar {.inline.} =
  return CBVar(valueType: Object, payload: CBVarPayload(objectValue: v, objectVendorId: vendorId.int32, objectTypeId: typeId.int32))

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
  
  proc registerChain*(chain: CBChainPtr) {.importcpp: "chainblocks::registerChain(#)", header: "runtime.hpp".}
  
  proc unregisterChain*(chain: CBChainPtr) {.importcpp: "chainblocks::unregisterChain(#)", header: "runtime.hpp".}

  proc add*(chain: CBChainPtr; blk: ptr CBRuntimeBlock) {.importcpp: "#.addBlock(#)", header: "runtime.hpp".}
  proc cbAddBlock*(chain: CBChainPtr; blk: ptr CBRuntimeBlock) {.cdecl, exportc, dynlib.} = add(chain, blk)

  proc globalVariable*(name: cstring): ptr CBVar {.importcpp: "chainblocks::globalVariable(#)", header: "runtime.hpp".}
  proc hasGlobalVariable*(name: cstring): bool {.importcpp: "chainblocks::hasGlobalVariable(#)", header: "runtime.hpp".}

  proc createNode*(): ptr CBNode {.noinline.} = cppnew(result, CBNode, CBNode)
  proc cbCreateNode*(): ptr CBNode {.cdecl, exportc, dynlib.} = createNode()
  proc destroyNode*(node: ptr CBNode) {.noinline.} = cppdel(node)
  proc cbDestroyNode*(node: ptr CBNode) {.cdecl, exportc, dynlib.} = destroyNode(node)
  proc schedule*(node: ptr CBNode, chain: CBChainPtr; input: CBVar = Empty) {.inline.} =
    node[].invoke("schedule", chain, input).to(void)
  # Notice this is like this due to code legacy (looped/unsafe), TODO
  proc cbSchedule*(node: ptr CBNode, chain: CBChainPtr; input: CBVar; loop: cint; unsafe: cint) {.cdecl, exportc, dynlib.} =
    chain[].looped = loop.bool
    chain[].unsafe = unsafe.bool
    schedule(node, chain, input)
  proc tick*(node: ptr CBNode, input: CBVar = Empty): bool {.inline, discardable.} =
    node[].invoke("tick", input).to(bool)
  proc cbNodeTick*(node: ptr CBNode, input: CBVar) {.cdecl, exportc, dynlib.} = tick(node, input)
  proc stop*(node: ptr CBNode) {.inline.} =
    node[].invoke("terminate").to(void)
  proc cbNodeStop*(node: ptr CBNode) {.cdecl, exportc, dynlib.} = stop(node)

  proc prepareInternal(chain: CBChainPtr) {.importcpp: "chainblocks::prepare(#)", header: "runtime.hpp".}
  proc prepare*(chain: CBChainPtr) {.inline.} =
    var frame = getFrameState()
    prepareInternal(chain)
    setFrameState(frame)
  proc cbPrepare*(chain: CBChainPtr) {.cdecl, exportc, dynlib.} = prepare(chain)

  proc startInternal(chain: CBChainPtr; input: CBVar) {.importcpp: "chainblocks::start(#, #)", header: "runtime.hpp".}
  # Notice this is like this due to code legacy (looped/unsafe), TODO
  proc start*(chain: CBChainPtr; looped, unsafe: bool = false, input: CBVar = Empty) {.inline.} =
    var frame = getFrameState()
    chain[].looped = looped
    chain[].unsafe = unsafe
    prepareInternal(chain)
    startInternal(chain, input)
    setFrameState(frame)
  # Notice this is like this due to code legacy (looped/unsafe), TODO
  proc cbStart*(chain: CBChainPtr; looped, unsafe: cint; input: CBVar) {.cdecl, exportc, dynlib.} =
    start(chain, looped.bool, unsafe.bool, input)
  
  proc tickInternal(chain: CBChainPtr; rootInput: CBVar = Empty) {.importcpp: "chainblocks::tick(#, #)", header: "runtime.hpp".}
  proc tick*(chain: CBChainPtr; rootInput: CBVar = Empty) {.inline.} =
    var frame = getFrameState()
    tickInternal(chain, rootInput)
    setFrameState(frame)
  proc cbTick*(chain: CBChainPtr; rootInput: CBVar) {.cdecl, exportc, dynlib.} = tick(chain, rootInput)
  
  proc stopInternal(chain: CBChainPtr; results: ptr CBVar) {.importcpp: "chainblocks::stop(#, #)", header: "runtime.hpp".}
  proc stop*(chain: CBChainPtr; results: ptr CBVar = nil) {.inline.} =
    var frame = getFrameState()
    stopInternal(chain, results)
    setFrameState(frame)
  proc get*(chain: CBChainPtr): CBVarConst {.inline.} = stop(chain, addr result.value)
  proc cbStop*(chain: CBChainPtr; results: ptr CBVar) {.cdecl, exportc, dynlib.} = stop(chain, results)
  
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
  
  proc store*(chain: CBChainPtr): string =
    let str = invokeFunction("chainblocks::store", chain).to(StdString)
    $(str.c_str().to(cstring))
  
  proc store*(v: CBVar): string =
    let str = invokeFunction("chainblocks::store", v).to(StdString)
    $(str.c_str().to(cstring))
  
  proc load*(chain: var CBChainPtr; jsonString: cstring) {.importcpp: "chainblocks::load(#, #)", header: "runtime.hpp".}
  proc load*(chain: var CBVar; jsonString: cstring) {.importcpp: "chainblocks::load(#, #)", header: "runtime.hpp".}

  defineCppType(BlocksMap, "phmap::node_hash_map<std::string, CBBlockConstructor>", "runtime.hpp")
  defineCppType(BlocksPair, "std::pair<std::string, CBBlockConstructor>", "runtime.hpp")

  var
    blocksRegister {.importcpp: "chainblocks::BlocksRegister", header: "runtime.hpp", nodecl.}: BlocksMap
    blockNamesCache: seq[string]
    blockNamesCacheSeq: CBStrings
  
  initSeq(blockNamesCacheSeq)

  proc cbBlocks*(): CBStrings {.cdecl, exportc, dynlib.} =
    invokeFunction("chainblocks::registerCoreBlocks").to(void)
    
    blockNamesCache.setLen(0)
    blockNamesCacheSeq.clear()
    for item in cppItems[BlocksMap, BlocksPair](blocksRegister):
      let itemName = item.first.c_str().to(cstring)
      blockNamesCache.add($itemName)
      blockNamesCacheSeq.push(blockNamesCache[^1].cstring)
    return blockNamesCacheSeq
    
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
      result = quote do:
        var
          `sym` = createBlock(`rtName`)
        `sym`.setup(`sym`)
        var
          params = `sym`.parameters(`sym`)
          `paramNames`: StbSeq[cstring]
        
        for param in params.mitems:
          `paramNames`.push(param.name)
      
      for i in 0..`args`.high:
        var
          val = `args`[i].value
          lab = newStrLitNode($`args`[i].label.strVal)
        result.add quote do:
          var
            cbVar: CBVar = `val`
            label = `lab`.cstring
            idx = `paramNames`.find(label)
          assert idx != -1, "Could not find parameter: " & $label
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
        
        var cbVar: CBVar = `args`
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
      
    if testCode != nil:
      result.add quote do:
        when defined blocksTesting:
          proc `testName`() =
            `testCode`
          `testName`()

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

  proc registerObjectType*(vendorId, typeId: FourCC; info: CBObjectInfo) =
    invokeFunction("chainblocks::registerObjectType", vendorId, typeId, info).to(void)

  proc registerEnumType*(vendorId, typeId: FourCC; info: CBEnumInfo) =
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

  proc newChain*(name: string): CBChainPtr {.inline.} =
    cppnew(result, CBChain, CBChain, name.cstring)
    registerChain(result)
  proc cbCreateChain*(name: cstring): CBChainPtr {.cdecl, exportc, dynlib.} =
    cppnew(result, CBChain, CBChain, name)
    registerChain(result)
  
  proc destroy*(chain: CBChainPtr) {.inline.} =
    unregisterChain(chain)
    cppdel(chain)
  proc cbDestroyChain*(chain: CBChainPtr) {.cdecl, exportc, dynlib.} = destroy(chain)
  
  proc runChain*(chain: CBChainPtr, context: ptr CBContextObj; chainInput: CBVar): RunChainOutput {.importcpp: "chainblocks::runChain(#, #, #)", header: "runtime.hpp".}

  proc activateBlock*(chain: ptr CBRuntimeBlock, context: ptr CBContextObj; input: var CBVar; output: var CBVar) {.importcpp: "chainblocks::activateBlock(#, #, #, #)", header: "runtime.hpp".}
  
  proc suspendInternal(seconds: float64): CBVar {.importcpp: "chainblocks::suspend(#)", header: "runtime.hpp".}
  proc suspend*(seconds: float64): CBVar {.inline.} =
    var frame = getFrameState()
    result = suspendInternal(seconds)
    setFrameState(frame)

  proc blocks*(chain: CBChainPtr): CBSeq {.importcpp: "chainblocks::blocks(#)", header: "runtime.hpp".}

  proc isRunning*(chain: CBChainPtr): bool {.importcpp: "chainblocks::isRunning(#)", header: "runtime.hpp".}

  proc sleep*(secs: float64) {.importcpp: "chainblocks::sleep(#)", header: "runtime.hpp".}
  proc cbSleep*(secs: float64) {.cdecl, exportc, dynlib.} = sleep(secs)

  proc canceled*(ctx: CBContext): bool {.inline.} = ctx.aborted
  
else:
  # When we are a dll with a collection of blocks!
  
  import dynlib
  
  type
    RegisterBlkProc = proc(name: cstring; initProc: CBBlockConstructor) {.cdecl.}
    RegisterTypeProc = proc(vendorId, typeId: FourCC; info: CBObjectInfo) {.cdecl.}
    RegisterEnumProc = proc(vendorId, typeId: FourCC; info: CBEnumInfo) {.cdecl.}
    RegisterLoopCb = proc(name: cstring; cb: CBCallback) {.cdecl.}
    UnregisterLoopCb = proc(name: cstring) {.cdecl.}
    CtxVariableProc = proc(ctx: CBContext; name: cstring): ptr CBVar {.cdecl.}
    CtxSetErrorProc = proc(ctx: CBContext; errorTxt: cstring) {.cdecl.}
    CtxStateProc = proc(ctx: CBContext): cint {.cdecl.}
    SuspendProc = proc(seconds: float64): CBVar {.cdecl.}
    StartTickChainProc = proc(chain: CBChainPtr; val: CBVar) {.cdecl.}
    StopChainProc = proc(chain: CBChainPtr; val: ptr CBVar) {.cdecl.}
    PrepareChainProc = proc(chain: CBChainPtr; looped, unsafe: bool) {.cdecl.}
    ActivateBlockProc = proc(blk: ptr CBRuntimeBlock; ctx: CBContext; input, output: ptr CBVar) {.cdecl.}
    CloneVarProc = proc(dst: ptr CBVar; src: ptr CBVar): int {.cdecl.}
    DestroyVarProc = proc(dst: ptr CBVar): int {.cdecl.}
  
  const cbRuntimeDll {.strdefine.} = ""
  when cbRuntimeDll != "":
    let exeLib = loadLib(cbRuntimeDll)
    static:
      echo "Using cb runtime: ", cbRuntimeDll
  else:
    let exeLib = loadLib()
  
  let
    cbRegisterBlock = cast[RegisterBlkProc](exeLib.symAddr("chainblocks_RegisterBlock"))
    cbRegisterObjectType = cast[RegisterTypeProc](exeLib.symAddr("chainblocks_RegisterObjectType"))
    cbRegisterEnumType = cast[RegisterEnumProc](exeLib.symAddr("chainblocks_RegisterEnumType"))
    cbRegisterLoopCb = cast[RegisterLoopCb](exeLib.symAddr("chainblocks_RegisterRunLoopCallback"))
    cbUnregisterLoopCb = cast[UnregisterLoopCb](exeLib.symAddr("chainblocks_UnregisterRunLoopCallback"))
    cbRegisterExitCb = cast[RegisterLoopCb](exeLib.symAddr("chainblocks_RegisterExitCallback"))
    cbUnregisterExitCb = cast[UnregisterLoopCb](exeLib.symAddr("chainblocks_UnregisterExitCallback"))
    cbContextVar = cast[CtxVariableProc](exeLib.symAddr("chainblocks_ContextVariable"))
    cbContextState = cast[CtxStateProc](exeLib.symAddr("chainblocks_ContextState"))
    cbSetError = cast[CtxSetErrorProc](exeLib.symAddr("chainblocks_SetError"))
    cbThrowException = cast[UnregisterLoopCb](exeLib.symAddr("chainblocks_ThrowException"))
    cbSuspend = cast[SuspendProc](exeLib.symAddr("chainblocks_Suspend"))
    cbActivateBlock = cast[ActivateBlockProc](exeLib.symAddr("chainblocks_ActivateBlock"))
    cbCloneVarProc = cast[CloneVarProc](exeLib.symAddr("chainblocks_CloneVar"))
    cbDestroyVarProc = cast[DestroyVarProc](exeLib.symAddr("chainblocks_DestroyVar"))

  proc registerBlock*(name: string; initProc: CBBlockConstructor) {.inline.} = cbRegisterBlock(name, initProc)
  proc registerObjectType*(vendorId, typeId: FourCC; info: CBObjectInfo) {.inline.} = cbRegisterObjectType(vendorId, typeId, info)
  proc registerEnumType*(vendorId, typeId: FourCC; info: CBEnumInfo) {.inline.} = cbRegisterEnumType(vendorId, typeId, info)
  proc registerRunLoopCallback*(name: string; callback: CBCallback) {.inline.} = cbRegisterLoopCb(name, callback)
  proc unregisterRunLoopCallback*(name: string) {.inline.} = cbUnregisterLoopCb(name)
  proc registerExitCallback*(name: string; callback: CBCallback) {.inline.} = cbRegisterExitCb(name, callback)
  proc unregisterExitCallback*(name: string) {.inline.} = cbUnregisterExitCb(name)

  proc contextVariable*(ctx: CBContext; name: cstring): ptr CBVar {.inline.} = cbContextVar(ctx, name)
  proc setError*(ctx: CBContext; errorTxt: cstring) {.inline.} = cbSetError(ctx, errorTxt)
  proc throwCBException*(msg: string) = cbThrowException(msg.cstring)
  proc canceled*(ctx: CBContext): bool {.inline.} = cbContextState(ctx) == 1
  
  proc suspend*(seconds: float64): CBVar {.inline.} =
    var frame = getFrameState()
    result = cbSuspend(seconds)
    setFrameState(frame)

  proc activateBlock*(chain: ptr CBRuntimeBlock, context: ptr CBContextObj; input: var CBVar; output: var CBVar) {.inline.} =
    cbActivateBlock(chain, context, addr input, addr output)

  proc cbCloneVar*(dst: var CBVar; src: var CBvar): int {.cdecl, dynlib, exportc.} = cbCloneVarProc(addr dst, addr src)
  proc cbDestroyVar*(dst: var CBVar): int {.cdecl, dynlib, exportc.} = cbDestroyVarProc(addr dst)

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

when appType != "lib" or defined(forceCBRuntime):
  # Swaps from compile time chain mode on/off
  macro setCompileTimeChain*(enabled: bool) = compileTimeMode = enabled.boolVal

  template compileTimeChain*(body: untyped): untyped =
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
      var `ctx`: CBContext

    for blkData in staticChainBlocks:
      var blk = blkData.blk
      result.add quote do:
        `currentOutput` = `blk`.activate(`ctx`, `currentOutput`)
    
    result.add quote do:
      `currentOutput`
    
  # Clear the compile time chain state
  macro clearCompileTimeChain*() = staticChainBlocks.setLen(0) # consume all

# always try this for safety
assert sizeof(CBVar) == 32
assert sizeof(CBVarPayload) == 16

when isMainModule and appType != "lib" and appType != "staticlib":
  import os
  
  type
    CBPow2Block = object
      inputValue: float
      params: array[1, CBVar]
  
  template inputTypes*(b: CBPow2Block): CBTypesInfo = { Float }
  template outputTypes*(b: CBPow2Block): CBTypesInfo = { Float }
  template parameters*(b: CBPow2Block): CBParametersInfo = *@[(cs"test", { Int })]
  template setParam*(b: CBPow2Block; index: int; val: CBVar) = b.params[0] = val
  template getParam*(b: CBPow2Block; index: int): CBVar = b.params[0]
  template preChain*(b: CBPow2Block; context: CBContext) = log "PreChain works if needed"
  template postChain*(b: CBPow2Block; context: CBContext) = log "PostChain works if needed"
  template activate*(b: CBPow2Block; context: CBContext; input: CBVar): CBVar = (input.payload.floatValue * input.payload.floatValue).CBVar

  chainblock CBPow2Block, "Pow2StaticBlock"

  proc run() =
    var
      pblock1: CBPow2Block
    pblock1.setup()
    var res1 = pblock1.activate(nil, 2.0)
    log res1.float
    
    var pblock2 = createBlock("Pow2StaticBlock")
    assert pblock2 != nil
    var param0Var = 10.CBVar
    log param0Var.valueType
    pblock2.setParam(pblock2, 0, param0Var)
    log pblock2.getParam(pblock2, 0).valueType
    var res2 = pblock2.activate(pblock2, nil, 2.0)
    log res2.float
    pblock2.preChain(pblock2, nil)
    pblock2.postChain(pblock2, nil)

    withChain inlineTesting:
      Const 10
      Math.Add 10
      Log()
    inlineTesting.start()
    assert inlineTesting.get() == 20.CBVar
    
    var f4seq: CBSeq
    initSeq(f4seq)
    f4seq.push (2.0, 3.0, 4.0, 5.0).CBVar
    f4seq.push (6.0, 7.0, 8.0, 9.0).CBVar
    withChain inlineTesting2:
      Const f4seq
      Math.Add (0.1, 0.2, 0.3, 0.4)
      Log()
      Math.Sqrt
      Log()
    inlineTesting2.start()
    
    var
      mainChain = newChain("mainChain")
    setCurrentChain(mainChain)

    withChain chain1:
      Msg "SubChain!"
      Sleep 0.2

    chain1.start(true)
    for i in 0..3:
      log "Iteration ", i
      chain1.tick()
      sleep(500)
    discard chain1.get()
    log "Stopped"
    chain1.tick() # should do nothing
    log "Done"

    var
      subChain1 = newChain("subChain1")
    setCurrentChain(subChain1)
    
    Const "Hey hey"
    Log()
    
    setCurrentChain(mainChain)
    
    var ifConst = createBlock("Const")
    ifConst[].setup(ifConst)
    ifConst[].setParam(ifConst, 0, "Hey hey")
    var ifLog = createBlock("Log")
    ifLog[].setup(ifLog)
    var ifBlocks = ~@[ifConst, ifLog]

    Log()
    Msg "Hello"
    Msg "World"
    Const 15
    If:
      Operator: CBVar(valueType: Enum, payload: CBVarPayload(enumValue: MoreEqual.CBEnum, enumVendorId: FragCC.int32, enumTypeId: BoolOpCC.int32))
      Operand: 10
      True: ifBlocks
    Sleep 0.0
    Const 11
    ToString()
    Log()
    
    assert mainChain.validate().len == 0
    
    mainChain.start(true)
    for i in 0..10:
      var idx = i.CBVar
      mainChain.tick(idx)
    
    mainChain.stop()

    mainChain.start(true)
    for i in 0..10:
      var idx = i.CBVar
      mainChain.tick(idx)
    
    let
      jstr = mainChain.store()
    assert jstr == """{"blocks":[{"name":"Log","params":[]},{"name":"Msg","params":[{"name":"Message","value":{"type":19,"value":"Hello"}}]},{"name":"Msg","params":[{"name":"Message","value":{"type":19,"value":"World"}}]},{"name":"Const","params":[{"name":"Value","value":{"type":5,"value":15}}]},{"name":"If","params":[{"name":"Operator","value":{"type":3,"typeId":1819242338,"value":3,"vendorId":1734439526}},{"name":"Operand","value":{"type":5,"value":10}},{"name":"True","value":{"type":22,"values":[{"name":"Const","params":[{"name":"Value","value":{"type":19,"value":"Hey hey"}}],"type":17},{"name":"Log","params":[],"type":17}]}},{"name":"False","value":{"type":22,"values":[]}},{"name":"Passthrough","value":{"type":4,"value":false}}]},{"name":"Sleep","params":[{"name":"Time","value":{"type":11,"value":0}}]},{"name":"Const","params":[{"name":"Value","value":{"type":5,"value":11}}]},{"name":"ToString","params":[]},{"name":"Log","params":[]}],"looped":true,"name":"mainChain","unsafe":false,"version":0.1}"""
    log jstr
    var jchain: CBChainPtr
    load(jchain, jstr)
    let
      jstr2 = jchain.store()
    log jstr2
    assert jstr2 == """{"blocks":[{"name":"Log","params":[]},{"name":"Msg","params":[{"name":"Message","value":{"type":19,"value":"Hello"}}]},{"name":"Msg","params":[{"name":"Message","value":{"type":19,"value":"World"}}]},{"name":"Const","params":[{"name":"Value","value":{"type":5,"value":15}}]},{"name":"If","params":[{"name":"Operator","value":{"type":3,"typeId":1819242338,"value":3,"vendorId":1734439526}},{"name":"Operand","value":{"type":5,"value":10}},{"name":"True","value":{"type":22,"values":[{"name":"Const","params":[{"name":"Value","value":{"type":19,"value":"Hey hey"}}],"type":17},{"name":"Log","params":[],"type":17}]}},{"name":"False","value":{"type":22,"values":[]}},{"name":"Passthrough","value":{"type":4,"value":false}}]},{"name":"Sleep","params":[{"name":"Time","value":{"type":11,"value":0}}]},{"name":"Const","params":[{"name":"Value","value":{"type":5,"value":11}}]},{"name":"ToString","params":[]},{"name":"Log","params":[]}],"looped":true,"name":"mainChain","unsafe":false,"version":0.1}"""
    
    var
      sm1 = ~@[0.1, 0.2, 0.3]
      sm2 = ~@[1.1, 1.2, 1.3]
      sm3 = ~@[6.1, 6.2, 6.3]
      sm4 = ~@[1, 2, 3]
    
    withChain testSeqMath:
      Const sm1
      Math.Add 1.0
      Log()
    
    testSeqMath.start()
    assert testSeqMath.get() == sm2
    testSeqMath.start()
    assert testSeqMath.get() == sm2
    
    withChain prepare:
      Const sm1
      SetVariable "v1"
    
    withChain consume:
      RunChain:
        Once: true
        Chain: prepare
      
      GetVariable "v1"
      Math.Add 1.0
      SetVariable "v1"
      Log()
    
    consume.start(true)
    for _ in 0..4: consume.tick()
    assert consume.get() == sm3
    consume.start(true)
    for _ in 0..4: consume.tick()
    assert consume.get() == sm3

    withChain testAdding:
      Const Empty
      AddVariable "v1"
      
      Const 1
      AddVariable "v1"
      Const 2
      AddVariable "v1"
      Const 3
      AddVariable "v1"
      
      GetVariable "v1"
      Log()
    
    testAdding.start(true)
    for _ in 0..4: testAdding.tick()
    assert testAdding.get() == sm4
    log "Restarting"
    testAdding.start(true)
    for _ in 0..4: testAdding.tick()
    assert testAdding.get() == sm4

    compileTimeChain:
      Msg "Hello"
      Msg "Static"
      Msg "World"
    
    prepareCompileTimeChain()
    discard synthesizeCompileTimeChain(Empty)
    clearCompileTimeChain()
    
    when false:
      # Useless benchmark... without Logging both would be 0
      # Used just to measure inline optimizations
      for _ in 0..9:
        withChain fibChainInit:
          Const 0
          SetVariable "a"
          Const 1
          SetVariable "b"

        withChain fibChain:
          RunChain:
            Chain: fibChainInit
            Once: true
          
          GetVariable "a"
          Log()         
          
          SwapVariables:
            Name1: "a"
            Name2: "b"

          GetVariable "a"
          Math.Add ~~"b"
          SetVariable "b"
        
        let cbStart = cpuTime()
        fibChain.start(true, true)
        for _ in 0..10:
          fibChain.tick()
        log "CB took: ", cpuTime() - cbStart
        discard fibChain.stop()
        destroy fibChain
        destroy fibChainInit

        iterator fib: int {.closure.} =
          var a = 0
          var b = 1
          while true:
            yield a
            swap a, b
            b = a + b
        
        let nimStart = cpuTime()
        var f = fib
        for i in 0..11:
          log f()
        log "Nim took: ", cpuTime() - nimStart
  
  run()
  
  log "Done"