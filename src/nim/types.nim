import nimline
import os

#[ 
  Since we will be used and consumed by many GC flavors of nim, no GC at all, other languages and what not
  the best choice for now is to use ptr rather then ref, so to allow all those scenarios, including modules dlls etc!

  Chains own blocks so the only pointers that need management is chain and context.

  Might become ref with proper working newruntime but so far I used so many hours for nothing
]#

const
  modulePath = currentSourcePath().splitPath().head
cppincludes(modulePath & "/../core")

type
  CBInt* {.importcpp: "CBInt", header: "chainblocks.hpp", nodecl.} = int64
  CBInt2* {.importcpp: "CBInt2", header: "chainblocks.hpp".} = object
  CBInt3* {.importcpp: "CBInt3", header: "chainblocks.hpp".} = object 
  CBInt4* {.importcpp: "CBInt4", header: "chainblocks.hpp".} = object 
  CBInt8* {.importcpp: "CBInt8", header: "chainblocks.hpp".} = object 
  CBInt16* {.importcpp: "CBInt16", header: "chainblocks.hpp".} = object 
  CBFloat* {.importcpp: "CBFloat", header: "chainblocks.hpp".} = float64
  CBFloat2* {.importcpp: "CBFloat2", header: "chainblocks.hpp".} = object 
  CBFloat3* {.importcpp: "CBFloat3", header: "chainblocks.hpp".} = object 
  CBFloat4* {.importcpp: "CBFloat4", header: "chainblocks.hpp".} = object 
  CBColor* {.importcpp: "CBColor", header: "chainblocks.hpp".} = object
    r*,g*,b*,a*: uint8
  CBPointer* {.importcpp: "CBPointer", header: "chainblocks.hpp".} = pointer
  CBString* {.importcpp: "CBString", header: "chainblocks.hpp".} = distinct cstring
  CBSeq* {.importcpp: "CBSeq", header: "chainblocks.hpp".} = ptr UncheckedArray[CBVar]
  CBTable* {.importcpp: "CBTable", header: "chainblocks.hpp".} = ptr UncheckedArray[CBNamedVar]
  CBStrings* {.importcpp: "CBStrings", header: "chainblocks.hpp".} = ptr UncheckedArray[CBString]
  CBEnum* {.importcpp: "CBEnum", header: "chainblocks.hpp".} = distinct int32

  CBNamedVar* {.importcpp: "CBNamedVar", header: "chainblocks.hpp".} = object
    key*: cstring
    value*: CBVar

  CBChain* {.importcpp: "CBChain", header: "chainblocks.hpp".} = object
    finishedOutput*: CBVar
    started*: bool
    finished*: bool
  CBChainPtr* = ptr CBChain

  CBContextObj* {.importcpp: "CBContext", header: "chainblocks.hpp".} = object
    aborted*: bool
    restarted*: bool
    stack*: CBSeq
  CBContext* = ptr CBContextObj

  CBEnumInfo* {.importcpp: "CBEnumInfo", header: "chainblocks.hpp".} = object
    name*: cstring
    labels*: CBStrings

  CBImage* {.importcpp: "CBImage", header: "chainblocks.hpp".} = object
    width*: uint16
    height*: uint16
    channels*: uint8
    data*: ptr UncheckedArray[uint8]

  CBType* {.importcpp: "CBType", header: "chainblocks.hpp", size: sizeof(uint8).} = enum
    None,
    Any,
    Object,
    Enum,
    Bool,
    Int,
    Int2, # A vector of 2 ints
    Int3, # A vector of 3 ints
    Int4, # A vector of 4 ints
    Int8,
    Int16,
    Float,
    Float2, # A vector of 2 floats
    Float3, # A vector of 3 floats
    Float4, # A vector of 4 floats
    String,
    Color, # A vector of 4 uint8
    Image,
    Seq,
    Table,
    Chain, # sub chains, e.g. IF/ELSE
    Block,
    ContextVar, # A string label to find from CBContext variables

  CBTypeInfo* {.importcpp: "CBTypeInfo", header: "chainblocks.hpp".} = object
    sequenced*: bool # This type can be in a sequence of itself
    basicType*: CBType
    objectVendorId*: int32
    objectTypeId*: int32
    enumVendorId*: int32
    enumTypeId*: int32

  CBTypesInfo* {.importcpp: "CBTypesInfo", header: "chainblocks.hpp".} = ptr UncheckedArray[CBTypeInfo]

  CBObjectInfo* {.importcpp: "CBObjectInfo", header: "chainblocks.hpp".} = object
    name*: cstring

  CBParameterInfo* {.importcpp: "CBParameterInfo", header: "chainblocks.hpp".} = object
    name*: cstring
    valueTypes*: CBTypesInfo
    allowContext*: bool # This parameter could be a contextvar
    help*: cstring
  
  CBParametersInfo* {.importcpp: "CBParametersInfo", header: "chainblocks.hpp".} = ptr UncheckedArray[CBParameterInfo]
  
  CBChainState* {.importcpp: "CBChainState", header: "chainblocks.hpp".} = enum
    Continue, # Even if None returned, continue to next block
    Restart, # Restart the chain from the top
    Stop # Stop the chain execution

  CBVar* {.importcpp: "CBVar", header: "chainblocks.hpp".} = object
    chainState*: CBChainState
    objectValue*: CBPointer
    objectVendorId*: int32
    objectTypeId*: int32
    boolValue*: bool
    intValue*: CBInt
    int2Value*: CBInt2
    int3Value*: CBInt3
    int4Value*: CBInt4
    int8Value*: CBInt8
    int16Value*: CBInt16
    floatValue*: CBFloat
    float2Value*: CBFloat2
    float3Value*: CBFloat3
    float4Value*: CBFloat4
    stringValue*: CBString
    colorValue*: CBColor
    imageValue*: CBImage
    seqValue*: CBSeq
    seqLen*: int32
    tableValue*: CBTable
    tableLen*: int32
    chainValue*: ptr CBChainPtr
    enumValue*: CBEnum
    enumVendorId*: int32
    enumTypeId*: int32
    valueType*: CBType
    reserved*: array[15, uint8]

  CBNameProc* {.importcpp: "CBNameProc", header: "chainblocks.hpp".} = proc(b: ptr CBRuntimeBlock): cstring {.cdecl.}
  CBHelpProc* {.importcpp: "CBHelpProc", header: "chainblocks.hpp".} = proc(b: ptr CBRuntimeBlock): cstring {.cdecl.}

  CBSetupProc* {.importcpp: "CBSetupProc", header: "chainblocks.hpp".} = proc(b: ptr CBRuntimeBlock) {.cdecl.}
  CBDestroyProc* {.importcpp: "CBDestroyProc", header: "chainblocks.hpp".} = proc(b: ptr CBRuntimeBlock) {.cdecl.}

  CBPreChainProc* {.importcpp: "CBPreChainProc", header: "chainblocks.hpp".} = proc(b: ptr CBRuntimeBlock; context: CBContext) {.cdecl.}
  CBPostChainProc* {.importcpp: "CBPostChainProc", header: "chainblocks.hpp".} = proc(b: ptr CBRuntimeBlock; context: CBContext) {.cdecl.}

  CBInputTypesProc*{.importcpp: "CBInputTypesProc", header: "chainblocks.hpp".}  = proc(b: ptr CBRuntimeBlock): CBTypesInfo {.cdecl.}
  CBOutputTypesProc* {.importcpp: "CBOutputTypesProc", header: "chainblocks.hpp".} = proc(b: ptr CBRuntimeBlock): CBTypesInfo {.cdecl.}

  CBExposedVariablesProc* {.importcpp: "CBExposedVariablesProc", header: "chainblocks.hpp".} = proc(b: ptr CBRuntimeBlock): CBParametersInfo {.cdecl.}
  CBConsumedVariablesProc* {.importcpp: "CBConsumedVariablesProc", header: "chainblocks.hpp".} = proc(b: ptr CBRuntimeBlock): CBParametersInfo {.cdecl.}

  CBParametersProc* {.importcpp: "CBParametersProc", header: "chainblocks.hpp".} = proc(b: ptr CBRuntimeBlock): CBParametersInfo {.cdecl.}
  CBSetParamProc* {.importcpp: "CBSetParamProc", header: "chainblocks.hpp".} = proc(b: ptr CBRuntimeBlock; index: int; val: CBVar) {.cdecl.}
  CBGetParamProc* {.importcpp: "CBGetParamProc", header: "chainblocks.hpp".} = proc(b: ptr CBRuntimeBlock; index: int): CBVar {.cdecl.}

  CBActivateProc* {.importcpp: "CBActivateProc", header: "chainblocks.hpp".} = proc(b: ptr CBRuntimeBlock; context: CBContext; input: CBVar): CBVar {.cdecl.}
  CBCleanupProc* {.importcpp: "CBCleanupProc", header: "chainblocks.hpp".} = proc(b: ptr CBRuntimeBlock) {.cdecl.}

  CBRuntimeBlock* {.importcpp: "CBRuntimeBlock", header: "chainblocks.hpp".} = object
    inlineBlockId*: uint8
    
    name*: CBNameProc
    help*: CBHelpProc
    
    setup*: CBSetupProc
    destroy*: CBDestroyProc
    
    preChain*: CBPreChainProc
    postChain*: CBPostChainProc
    
    inputTypes*: CBInputTypesProc
    outputTypes*: CBOutputTypesProc
    
    exposedVariables*: CBExposedVariablesProc
    consumedVariables*: CBConsumedVariablesProc
    
    parameters*: CBParametersProc
    setParam*: CBSetParamProc
    getParam*: CBGetParamProc

    activate*: CBActivateProc
    cleanup*: CBCleanupProc

  CBBlockConstructor* {.importcpp: "CBBlockConstructor", header: "chainblocks.hpp".} = proc(): ptr CBRuntimeBlock {.cdecl.}

  CBOnRunLoopTick* {.importcpp: "CBOnRunLoopTick", header: "chainblocks.hpp".} = proc(): void {.cdecl.}

  CBSeqLike* = CBSeq | CBTypesInfo | CBParametersInfo | CBStrings
  CBIntVectorsLike* = CBInt2 | CBInt3 | CBInt4
  CBFloatVectorsLike* = CBFloat2 | CBFloat3 | CBFloat4

registerCppType CBChain
registerCppType CBContextObj
registerCppType CBInt2
registerCppType CBInt3
registerCppType CBInt4
registerCppType CBFloat2
registerCppType CBFloat3
registerCppType CBFloat4

var
  Empty* = CBVar(valueType: None, chainState: Continue)
  RestartChain* = CBVar(valueType: None, chainState: Restart)
  StopChain* = CBVar(valueType: None, chainState: Stop)

# Vectors
proc `[]`*(v: CBIntVectorsLike; index: int): int64 {.inline, noinit.} = v.toCpp[index].to(int64)
proc `[]=`*(v: var CBIntVectorsLike; index: int; value: int64) {.inline.} = v.toCpp[index] = value
proc `[]`*(v: CBFloatVectorsLike; index: int): float64 {.inline, noinit.} = v.toCpp[index].to(float64)
proc `[]=`*(v: var CBFloatVectorsLike; index: int; value: float64) {.inline.} = v.toCpp[index] = value

# CBTable
proc initTable*(t: var CBTable) {.inline.} =
  t = nil
  invokeFunction("stbds_shdefault", t, Empty).to(void)
proc freeTable*(t: var CBTable) {.inline.} = invokeFunction("stbds_shfree", t).to(void)
proc len*(t: CBTable): int {.inline.} = invokeFunction("stbds_shlen", t).to(int)
iterator mitems*(t: CBTable): var CBNamedVar {.inline.} =
  for i in 0..<t.len:
    yield t[i]
proc incl*(t: CBTable; pair: CBNamedVar) {.inline.} = invokeFunction("stbds_shputs", t, pair).to(void)
proc incl*(t: CBTable; k: cstring; v: CBVar) {.inline.} = incl(t, CBNamedVar(key: k, value: v))
proc excl*(t: CBTable; key: cstring) {.inline.} = invokeFunction("stbds_shdel", t, key).to(void)
proc find*(t: CBTable; key: cstring): int {.inline.} = invokeFunction("stbds_shgeti", t, key).to(int)
converter toCBVar*(t: CBTable): CBVar {.inline.} = CBVar(valueType: Table, tableValue: t)

# CBSeqLikes
proc initSeq*(s: var CBSeqLike) {.inline.} = s = nil
proc freeSeq*(cbs: var CBSeqLike) {.inline.} = invokeFunction("stbds_arrfree", cbs).to(void)
proc len*(s: CBSeqLike): int {.inline.} = invokeFunction("stbds_arrlen", s).to(int)
iterator mitems*(s: CBSeq): var CBVar {.inline.} =
  for i in 0..<s.len:
    yield s[i]
iterator mitems*(s: CBTypesInfo): var CBTypeInfo {.inline.} =
  for i in 0..<s.len:
    yield s[i]
iterator mitems*(s: CBParametersInfo): var CBParameterInfo {.inline.} =
  for i in 0..<s.len:
    yield s[i]
iterator mitems*(s: CBStrings): var CBString {.inline.} =
  for i in 0..<s.len:
    yield s[i]
proc push*[T](cbs: var CBSeqLike, val: T) {.inline.} = invokeFunction("stbds_arrpush", cbs, val).to(void)
proc push*(cbs: var CBSeq, val: CBVar) {.inline.} = invokeFunction("stbds_arrpush", cbs, val).to(void)
proc pop*(cbs: CBSeq): CBVar {.inline.} = invokeFunction("stbds_arrpop", cbs).to(CBVar)
proc clear*(cbs: var CBSeqLike) {.inline.} = invokeFunction("stbds_arrsetlen", cbs, 0).to(void)
proc clear*(cbs: var CBSeq) {.inline.} = invokeFunction("stbds_arrsetlen", cbs, 0).to(void)
iterator items*(s: CBParametersInfo): CBParameterInfo {.inline.} =
  for i in 0..<s.len:
    yield s[i]
iterator items*(s: CBSeq): CBVar {.inline.} =
  for i in 0..<s.len:
    yield s[i]
iterator items*(s: CBStrings): CBString {.inline.} =
  for i in 0..<s.len:
    yield s[i]

# Strings
converter toCBStrings*(strings: var seq[string]): CBStrings {.inline.} =
  # strings must be kept alive!
  initSeq(result)
  for str in strings.mitems:
    result.push str.cstring

proc `$`*(s: CBString): string {.inline.} = $cast[cstring](s)
converter toString*(s: CBString): string {.inline.} = $cast[cstring](s)
converter toString*(s: string): CBString {.inline.} = cast[CBString](s.cstring)
converter toStringVar*(s: string): CBVar {.inline.} =
  result.valueType = String
  result.stringValue = cast[CBString](s.cstring)

# Memory utilities to cache stuff around
type
  CachedVarValues* = object
    strings: seq[string]
    seqs: seq[CBSeq]

proc destroy*(cache: var CachedVarValues) =
  for s in cache.seqs.mitems:
    freeSeq(s)
  # Force deallocs
  when defined nimV2:
    cache.strings = @[]
    cache.seqs = @[]

proc clone*(v: CBVar; cache: var CachedVarValues): CBVar {.inline.} =
  # Need to add image support if we ever have image parameters! TODO
  if v.valueType == String:
    result.valueType = String
    cache.strings.add(v.stringValue)
    result.stringValue = cache.strings[^1].cstring.CBString
  elif v.valueType == Seq:
    result.valueType = Seq
    initSeq(result.seqValue)
    for item in v.seqValue.mitems:
      result.seqValue.push item.clone(cache)
    cache.seqs.add(result.seqValue)
  else:
    result = v

proc clone*(v: CBSeq; cache: var CachedVarValues): CBSeq {.inline.} =
  initSeq(result)
  for item in v.mitems:
    result.push item.clone(cache)

# Leave them last cos VScode highlight will freak out..

# Exception
{.emit: "#include <runtime.hpp>".}

proc throwCBException*(msg: string) = emitc("throw chainblocks::CBException(", msg.cstring, ");")

# Allocators using cpp to properly construct in C++ fashion (we have some blocks that need this)
template cppnew*(pt, typ1, typ2: untyped): untyped = emitc(`pt`, " = reinterpret_cast<", `typ1`, "*>(new ", `typ2`, "());")
template cppnew*(pt, typ1, typ2, a1: untyped): untyped = emitc(`pt`, " = reinterpret_cast<", `typ1`, "*>(new ", `typ2`, "(", `a1`, "));")
template cppnew*(pt, typ1, typ2, a1, a2: untyped): untyped = emitc(`pt`, " = reinterpret_cast<", `typ1`, "*>(new ", `typ2`, "(", `a1`, ", ", `a2`, "));")
template cppnew*(pt, typ1, typ2, a1, a2, a3: untyped): untyped = emitc(`pt`, " = reinterpret_cast<", `typ1`, "*>(new ", `typ2`, "(", `a1`, ", ", `a2`, ", ", `a3`, "));")
template cppdel*(pt: untyped): untyped = emitc("delete ", `pt`, ";")
