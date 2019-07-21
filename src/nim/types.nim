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
    returned*: bool
    node*: ptr CBNode
  CBChainPtr* = ptr CBChain

  CBNode* {.importcpp: "CBNode", header: "chainblocks.hpp".} = object

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
    None
    Any
    Object
    Enum
    Bool
    Int
    Int2 # A vector of 2 ints
    Int3 # A vector of 3 ints
    Int4 # A vector of 4 ints
    Int8
    Int16
    Float
    Float2 # A vector of 2 floats
    Float3 # A vector of 3 floats
    Float4 # A vector of 4 floats
    Color # A vector of 4 uint8
    Chain # sub chains, e.g. IF/ELSE
    Block
    
    EndOfBlittableTypes
    
    String
    ContextVar # A string label to find from CBContext variables
    Image
    Seq
    Table

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

  CBVarPayload* {.importcpp: "CBVarPayload", header: "chainblocks.hpp".} = object
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
    chainValue*: CBChainPtr
    blockValue*: ptr CBRuntimeBlock
    enumValue*: CBEnum
    enumVendorId*: int32
    enumTypeId*: int32

  CBVar* {.importcpp: "CBVar", header: "chainblocks.hpp".} = object
    payload*: CBVarPayload
    valueType*: CBType
    reserved*: array[15, uint8]

  CBVarConst* = object
    value*: CBVar

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

  CBCallback* {.importcpp: "CBCallback", header: "chainblocks.hpp".} = proc(): void {.cdecl.}

  CBSeqLike* = CBSeq | CBTypesInfo | CBParametersInfo | CBStrings
  CBIntVectorsLike* = CBInt2 | CBInt3 | CBInt4 | CBInt8 | CBInt16
  CBFloatVectorsLike* = CBFloat2 | CBFloat3 | CBFloat4

proc `~quickcopy`*(clonedVar: var CBVar): int {.importcpp: "chainblocks::destroyVar(#)", header: "runtime.hpp", discardable.}
proc quickcopy*(dst: var CBVar; src: var CBvar): int {.importcpp: "chainblocks::cloneVar(#, #)", header: "runtime.hpp", discardable.}
proc `=destroy`*(v: var CBVarConst) {.inline.} = discard `~quickcopy` v.value

var AllIntTypes* = { Int, Int2, Int3, Int4, Int8, Int16 }
var AllFloatTypes* = { Float, Float2, Float3, Float4 }

template chainState*(v: CBVar): auto = v.payload.chainState
template objectValue*(v: CBVar): auto = v.payload.objectValue
template objectVendorId*(v: CBVar): auto = v.payload.objectVendorId
template objectTypeId*(v: CBVar): auto = v.payload.objectTypeId
template boolValue*(v: CBVar): auto = v.payload.boolValue
template intValue*(v: CBVar): auto = v.payload.intValue
template int2Value*(v: CBVar): auto = v.payload.int2Value
template int3Value*(v: CBVar): auto = v.payload.int3Value
template int4Value*(v: CBVar): auto = v.payload.int4Value
template int8Value*(v: CBVar): auto = v.payload.int8Value
template int16Value*(v: CBVar): auto = v.payload.int16Value
template floatValue*(v: CBVar): auto = v.payload.floatValue
template float2Value*(v: CBVar): auto = v.payload.float2Value
template float3Value*(v: CBVar): auto = v.payload.float3Value
template float4Value*(v: CBVar): auto = v.payload.float4Value
template stringValue*(v: CBVar): auto = v.payload.stringValue
template colorValue*(v: CBVar): auto = v.payload.colorValue
template imageValue*(v: CBVar): auto = v.payload.imageValue
template seqValue*(v: CBVar): auto = v.payload.seqValue
template seqLen*(v: CBVar): auto = v.payload.seqLen
template tableValue*(v: CBVar): auto = v.payload.tableValue
template tableLen*(v: CBVar): auto = v.payload.tableLen
template chainValue*(v: CBVar): auto = v.payload.chainValue
template blockValue*(v: CBVar): auto = v.payload.blockValue
template enumValue*(v: CBVar): auto = v.payload.enumValue
template enumVendorId*(v: CBVar): auto = v.payload.enumVendorId
template enumTypeId*(v: CBVar): auto = v.payload.enumTypeId

template valueType*(v: CBVarConst): auto = v.value.valueType
template chainState*(v: CBVarConst): auto = v.value.payload.chainState
template objectValue*(v: CBVarConst): auto = v.value.payload.objectValue
template objectVendorId*(v: CBVarConst): auto = v.value.payload.objectVendorId
template objectTypeId*(v: CBVarConst): auto = v.value.payload.objectTypeId
template boolValue*(v: CBVarConst): auto = v.value.payload.boolValue
template intValue*(v: CBVarConst): auto = v.value.payload.intValue
template int2Value*(v: CBVarConst): auto = v.value.payload.int2Value
template int3Value*(v: CBVarConst): auto = v.value.payload.int3Value
template int4Value*(v: CBVarConst): auto = v.value.payload.int4Value
template int8Value*(v: CBVarConst): auto = v.value.payload.int8Value
template int16Value*(v: CBVarConst): auto = v.value.payload.int16Value
template floatValue*(v: CBVarConst): auto = v.value.payload.floatValue
template float2Value*(v: CBVarConst): auto = v.value.payload.float2Value
template float3Value*(v: CBVarConst): auto = v.value.payload.float3Value
template float4Value*(v: CBVarConst): auto = v.value.payload.float4Value
template stringValue*(v: CBVarConst): auto = v.value.payload.stringValue
template colorValue*(v: CBVarConst): auto = v.value.payload.colorValue
template imageValue*(v: CBVarConst): auto = v.value.payload.imageValue
template seqValue*(v: CBVarConst): auto = v.value.payload.seqValue
template seqLen*(v: CBVarConst): auto = value.v.payload.seqLen
template tableValue*(v: CBVarConst): auto = v.value.payload.tableValue
template tableLen*(v: CBVarConst): auto = v.value.payload.tableLen
template chainValue*(v: CBVarConst): auto = v.value.payload.chainValue
template blockValue*(v: CBVar): auto = v.value.payload.blockValue
template enumValue*(v: CBVarConst): auto = v.value.payload.enumValue
template enumVendorId*(v: CBVarConst): auto = v.value.payload.enumVendorId
template enumTypeId*(v: CBVarConst): auto = v.value.payload.enumTypeId

template `chainState=`*(v: CBVar, val: auto) = v.payload.chainState = val
template `objectValue=`*(v: CBVar, val: auto) = v.payload.objectValue = val
template `objectVendorId=`*(v: CBVar, val: auto) = v.payload.objectVendorId = val
template `objectTypeId=`*(v: CBVar, val: auto) = v.payload.objectTypeId = val
template `boolValue=`*(v: CBVar, val: auto) = v.payload.boolValue = val
template `intValue=`*(v: CBVar, val: auto) = v.payload.intValue = val
template `int2Value=`*(v: CBVar, val: auto) = v.payload.int2Value = val
template `int3Value=`*(v: CBVar, val: auto) = v.payload.int3Value = val
template `int4Value=`*(v: CBVar, val: auto) = v.payload.int4Value = val
template `int8Value=`*(v: CBVar, val: auto) = v.payload.int8Value = val
template `int16Value=`*(v: CBVar, val: auto) = v.payload.int16Value = val
template `floatValue=`*(v: CBVar, val: auto) = v.payload.floatValue = val
template `float2Value=`*(v: CBVar, val: auto) = v.payload.float2Value = val
template `float3Value=`*(v: CBVar, val: auto) = v.payload.float3Value = val
template `float4Value=`*(v: CBVar, val: auto) = v.payload.float4Value = val
template `stringValue=`*(v: CBVar, val: auto) = v.payload.stringValue = val
template `colorValue=`*(v: CBVar, val: auto) = v.payload.colorValue = val
template `imageValue=`*(v: CBVar, val: auto) = v.payload.imageValue = val
template `seqValue=`*(v: CBVar, val: auto) = v.payload.seqValue = val
template `seqLen=`*(v: CBVar, val: auto) = v.payload.seqLen = val
template `tableValue=`*(v: CBVar, val: auto) = v.payload.tableValue = val
template `tableLen=`*(v: CBVar, val: auto) = v.payload.tableLen = val
template `chainValue=`*(v: CBVar, val: auto) = v.payload.chainValue = val
template `blockValue=`*(v: CBVar, val: auto) = v.payload.blockValue = val
template `enumValue=`*(v: CBVar, val: auto) = v.payload.enumValue = val
template `enumVendorId=`*(v: CBVar, val: auto) = v.payload.enumVendorId = val
template `enumTypeId=`*(v: CBVar, val: auto) = v.payload.enumTypeId = val

registerCppType CBChain
registerCppType CBNode
registerCppType CBContextObj
registerCppType CBInt2
registerCppType CBInt3
registerCppType CBInt4
registerCppType CBInt8
registerCppType CBInt16
registerCppType CBFloat2
registerCppType CBFloat3
registerCppType CBFloat4

var
  Empty* = CBVar(valueType: None, payload: CBVarPayload(chainState: Continue))
  RestartChain* = CBVar(valueType: None, payload: CBVarPayload(chainState: Restart))
  StopChain* = CBVar(valueType: None, payload: CBVarPayload(chainState: Stop))

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
proc incl*(t: var CBTable; pair: CBNamedVar) {.inline.} = invokeFunction("stbds_shputs", t, pair).to(void)
proc incl*(t: var CBTable; k: cstring; v: CBVar) {.inline.} = incl(t, CBNamedVar(key: k, value: v))
proc excl*(t: CBTable; key: cstring) {.inline.} = invokeFunction("stbds_shdel", t, key).to(void)
proc find*(t: CBTable; key: cstring): int {.inline.} = invokeFunction("stbds_shgeti", t, key).to(int)
converter toCBVar*(t: CBTable): CBVar {.inline.} = CBVar(valueType: Table, payload: CBVarPayload(tableValue: t))

# CBSeqLikes
proc initSeq*(s: var CBSeqLike) {.inline.} = s = nil
proc freeSeq*(cbs: var CBSeqLike) {.inline.} = invokeFunction("stbds_arrfree", cbs).to(void)
proc freeSeq*(cbs: var CBSeq) {.inline.} = invokeFunction("stbds_arrfree", cbs).to(void)
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
proc pop*(cbs: var CBSeq): CBVar {.inline.} = invokeFunction("stbds_arrpop", cbs).to(CBVar)
proc clear*(cbs: var CBSeqLike) {.inline.} = invokeFunction("stbds_arrsetlen", cbs, 0).to(void)
proc clear*(cbs: var CBSeq) {.inline.} = invokeFunction("stbds_arrsetlen", cbs, 0).to(void)
proc setLen*(cbs: var CBSeq; newLen: int) {.inline.} = invokeFunction("stbds_arrsetlen", cbs, newLen).to(void)
iterator items*(s: CBParametersInfo): CBParameterInfo {.inline.} =
  for i in 0..<s.len:
    yield s[i]
iterator items*(s: CBSeq): CBVar {.inline.} =
  for i in 0..<s.len:
    yield s[i]
iterator items*(s: CBStrings): CBString {.inline.} =
  for i in 0..<s.len:
    yield s[i]
proc `~@`*[IDX, CBVar](a: array[IDX, CBVar]): CBSeq =
  initSeq(result)
  for v in a:
    result.push v

# Strings
converter toCBStrings*(strings: var seq[string]): CBStrings {.inline.} =
  # strings must be kept alive!
  initSeq(result)
  for str in strings.mitems:
    result.push str.cstring

proc `$`*(s: CBString): string {.inline.} = $cast[cstring](s)
converter toString*(s: CBString): string {.inline.} = $s.cstring
converter toString*(s: string): CBString {.inline.} = s.cstring.CBString
converter toStringVar*(s: string): CBVar {.inline.} =
  result.valueType = String
  result.payload.stringValue = s.cstring.CBString

# Exception
{.emit: "#include <runtime.hpp>".}

proc throwCBException*(msg: string) = emitc("throw chainblocks::CBException(", msg.cstring, ");")

# Allocators using cpp to properly construct in C++ fashion (we have some blocks that need this)
template cppnew*(pt, typ1, typ2: untyped): untyped = emitc(`pt`, " = reinterpret_cast<", `typ1`, "*>(new ", `typ2`, "());")
template cppnew*(pt, typ1, typ2, a1: untyped): untyped = emitc(`pt`, " = reinterpret_cast<", `typ1`, "*>(new ", `typ2`, "(", `a1`, "));")
template cppnew*(pt, typ1, typ2, a1, a2: untyped): untyped = emitc(`pt`, " = reinterpret_cast<", `typ1`, "*>(new ", `typ2`, "(", `a1`, ", ", `a2`, "));")
template cppnew*(pt, typ1, typ2, a1, a2, a3: untyped): untyped = emitc(`pt`, " = reinterpret_cast<", `typ1`, "*>(new ", `typ2`, "(", `a1`, ", ", `a2`, ", ", `a3`, "));")
template cppdel*(pt: untyped): untyped = emitc("delete ", `pt`, ";")
