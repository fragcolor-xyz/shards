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
  CBFloat* {.importcpp: "CBFloat", header: "chainblocks.hpp".} = float64
  CBFloat2* {.importcpp: "CBFloat2", header: "chainblocks.hpp".} = object 
  CBFloat3* {.importcpp: "CBFloat3", header: "chainblocks.hpp".} = object 
  CBFloat4* {.importcpp: "CBFloat4", header: "chainblocks.hpp".} = object 
  CBColor* {.importcpp: "CBColor", header: "chainblocks.hpp".} = object
    r*,g*,b*,a*: uint8
  CBPointer* {.importcpp: "CBPointer", header: "chainblocks.hpp".} = pointer
  CBString* {.importcpp: "CBString", header: "chainblocks.hpp".} = distinct pointer
  CBSeq* {.importcpp: "CBSeq", header: "chainblocks.hpp".} = object

  CBChain* {.importcpp: "CBChain", header: "chainblocks.hpp".} = object
    finishedOutput*: CBVar
    started*: bool
    finished*: bool
  CBContextObj* {.importcpp: "CBContext", header: "chainblocks.hpp".} = object
    aborted*: bool
    restarted*: bool
    stack*: CBSeq
  CBContext* = ptr CBContextObj

  CBBoolOp* {.importcpp: "CBBoolOp", header: "chainblocks.hpp".} = enum
    Equal,
    More,
    Less,
    MoreEqual,
    LessEqual

  CBImage* {.importcpp: "CBImage", header: "chainblocks.hpp".} = object
    width*: int32
    height*: int32
    channels*: int32
    data*: ptr UncheckedArray[uint8]

  CBType* {.importcpp: "CBType", header: "chainblocks.hpp", size: sizeof(uint8).} = enum
    None,
    Any,
    Object,
    Bool,
    Int,
    Int2, # A vector of 2 ints
    Int3, # A vector of 3 ints
    Int4, # A vector of 4 ints
    Float,
    Float2, # A vector of 2 floats
    Float3, # A vector of 3 floats
    Float4, # A vector of 4 floats
    String,
    Color, # A vector of 4 uint8
    Image,
    BoolOp,
    Seq, 
    Chain, # sub chains, e.g. IF/ELSE
    ContextVar, # A string label to find from CBContext variables

  CBTypeInfo* {.importcpp: "CBTypeInfo", header: "chainblocks.hpp".} = object
    sequenced*: bool # This type can be in a sequence of itself
    basicType*: CBType
    objectVendorId*: int32
    objectTypeId*: int32
    seqTypes*: CBTypesInfo

  CBTypesInfo* {.importcpp: "CBTypesInfo", header: "chainblocks.hpp".} = object

  CBObjectInfo* {.importcpp: "CBObjectInfo", header: "chainblocks.hpp".} = object
    name*: CBString

  CBParameterInfo* {.importcpp: "CBParameterInfo", header: "chainblocks.hpp".} = object
    name*: CBString
    valueTypes*: CBTypesInfo
    allowContext*: bool # This parameter could be a contextvar
    help*: CBString
  
  CBParametersInfo* {.importcpp: "CBParametersInfo", header: "chainblocks.hpp".} = object
  
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
    floatValue*: CBFloat
    float2Value*: CBFloat2
    float3Value*: CBFloat3
    float4Value*: CBFloat4
    stringValue*: CBString
    colorValue*: CBColor
    imageValue*: CBImage
    seqValue*: CBSeq
    chainValue*: ptr CBChain
    boolOpValue*: CBBoolOp
    valueType*: CBType

  CBRuntimeBlock* {.importcpp: "CBRuntimeBlock", header: "chainblocks.hpp", inheritable.} = object
    name*: proc(b: ptr CBRuntimeBlock): CBString {.cdecl.}
    help*: proc(b: ptr CBRuntimeBlock): CBString {.cdecl.}
    
    setup*: proc(b: ptr CBRuntimeBlock) {.cdecl.}
    destroy*: proc(b: ptr CBRuntimeBlock) {.cdecl.}
    
    preChain*: proc(b: ptr CBRuntimeBlock; context: CBContext) {.cdecl.}
    postChain*: proc(b: ptr CBRuntimeBlock; context: CBContext) {.cdecl.}
    
    inputTypes*: proc(b: ptr CBRuntimeBlock): CBTypesInfo {.cdecl.}
    outputTypes*: proc(b: ptr CBRuntimeBlock): CBTypesInfo {.cdecl.}
    
    exposedVariables*: proc(b: ptr CBRuntimeBlock): CBParametersInfo {.cdecl.}
    consumedVariables*: proc(b: ptr CBRuntimeBlock): CBParametersInfo {.cdecl.}
    
    parameters*: proc(b: ptr CBRuntimeBlock): CBParametersInfo {.cdecl.}
    setParam*: proc(b: ptr CBRuntimeBlock; index: int; val: CBVar) {.cdecl.}
    getParam*: proc(b: ptr CBRuntimeBlock; index: int): CBVar {.cdecl.}

    activate*: proc(b: ptr CBRuntimeBlock; context: CBContext; input: CBVar): CBVar {.cdecl.}
    cleanup*: proc(b: ptr CBRuntimeBlock) {.cdecl.}

  CBBlockConstructor* {.importcpp: "CBBlockConstructor", header: "chainblocks.hpp".} = proc(): ptr CBRuntimeBlock {.cdecl.}

  CBSeqLike* = CBSeq | CBTypesInfo | CBParametersInfo
  CBIntVectorsLike* = CBInt2 | CBInt3 | CBInt4
  CBFloatVectorsLike* = CBFloat2 | CBFloat3 | CBFloat4

registerCppType CBTypeInfo
registerCppType CBTypesInfo
registerCppType CBString
registerCppType CBParameterInfo
registerCppType CBParametersInfo
registerCppType CBSeq
registerCppType CBChain
registerCppType CBContextObj
registerCppType CBInt2
registerCppType CBInt3
registerCppType CBInt4
registerCppType CBFloat2
registerCppType CBFloat3
registerCppType CBFloat4

proc free*(v: CBVar) {.importcpp: "#.free()".}

proc `[]`*(v: CBIntVectorsLike; index: int): int64 {.inline, noinit.} = v.toCpp[index].to(int64)
proc `[]=`*(v: var CBIntVectorsLike; index: int; value: int64) {.inline.} = v.toCpp[index] = value
proc `[]`*(v: CBFloatVectorsLike; index: int): float64 {.inline, noinit.} = v.toCpp[index].to(float64)
proc `[]=`*(v: var CBFloatVectorsLike; index: int; value: float64) {.inline.} = v.toCpp[index] = value

proc len*(tinfo: CBSeqLike): int {.inline.} = invokeFunction("da_count", tinfo).to(int)
proc initSeq*(cbs: var CBSeqLike) {.inline.} = invokeFunction("da_init", cbs).to(void)
proc freeSeq*(cbs: var CBSeqLike) {.inline.} = invokeFunction("da_free", cbs).to(void)
proc initSeq*(cbs: var CBSeq) {.inline.} = invokeFunction("da_init", cbs).to(void)
proc getItemRef*(tinfo: CBSeq; index: int): var CBVar {.inline, noinit.} = invokeFunction("da_getptr", tinfo, index).to(ptr CBVar)[]
iterator mitems*(s: CBSeq): var CBVar {.inline.} =
  for i in 0..<s.len:
    yield getItemRef(s, i)
proc freeSeq*(cbs: var CBSeq) {.inline.} =
  for item in cbs.mitems: item.free()
  invokeFunction("da_free", cbs).to(void)
proc push*[T](cbs: var CBSeqLike, val: T) {.inline.} = invokeFunction("da_push", cbs, val).to(void)
proc push*(cbs: var CBSeq, val: CBVar) {.inline.} = invokeFunction("da_push", cbs, val).to(void)
proc pop*(cbs: var CBSeq): CBVar {.inline.} = invokeFunction("da_pop", cbs).to(CBVar)
proc clear*(s: var CBSeqLike) {.inline.} = invokeFunction("da_clear", s).to(void)
proc clear*(s: var CBSeq) {.inline.} = invokeFunction("da_clear", s).to(void)
proc `[]`*(tinfo: CBSeq; index: int): CBVar {.inline, noinit.} = invokeFunction("da_get", tinfo, index).to(CBVar)
proc `[]=`*(tinfo: var CBSeq; index: int; value: CBVar) {.inline.} = invokeFunction("da_set", tinfo, index, value).to(void)
proc `[]`*(tinfo: CBTypesInfo; index: int): CBTypeInfo {.inline, noinit.} = invokeFunction("da_get", tinfo, index).to(CBTypeInfo)
proc `[]=`*(tinfo: var CBTypesInfo; index: int; value: CBTypeInfo) {.inline.} = invokeFunction("da_set", tinfo, index, value).to(void)
proc `[]`*(tinfo: CBParametersInfo; index: int): CBParameterInfo {.inline, noinit.} = invokeFunction("da_get", tinfo, index).to(CBParameterInfo)
proc `[]=`*(tinfo: var CBParametersInfo; index: int; value: CBParameterInfo) {.inline.} = invokeFunction("da_set", tinfo, index, value).to(void)
iterator items*(s: CBParametersInfo): CBParameterInfo {.inline.} =
  for i in 0..<s.len:
    yield s[i]
iterator items*(s: CBSeq): CBVar {.inline.} =
  for i in 0..<s.len:
    yield s[i]

proc `$`*(s: CBString): string {.inline.} = $cast[cstring](s)
proc newString*(txt: string): CBString {.inline.} =
  # echo "newString: ", txt
  invokeFunction("gb_make_string", txt.cstring).to(CBString)
proc freeString*(cbStr: CBString) {.inline.} =
  # echo "freeString: ", cbStr
  invokeFunction("gb_free_string", cbStr).to(void)
proc setString*(cbStr: CBString; s: string): CBString {.inline.} = invokeFunction("gb_set_string", cbStr, s.cstring).to(CBString)
converter toString*(s: CBString): string {.inline.} = $cast[cstring](s)
converter toString*(s: string): CBString {.inline.} =
  # echo "Quick string convert...: ", s
  cast[CBString](s.cstring)
converter toStringVar*(s: string): CBVar {.inline.} =
  # echo "Quick string convert...: ", s
  result.valueType = String
  result.stringValue = cast[CBString](s.cstring)

proc clone*(v: CBVar): CBVar {.inline.} =
  if v.valueType == String:
    result.valueType = String
    result.stringValue = invokeFunction("gb_make_string", v.stringValue).to(CBString)
  elif v.valueType == Seq:
    result.valueType = Seq
    initSeq(result.seqValue)
    for item in v.seqValue.mitems:
      result.seqValue.push item.clone()
  else:
    result = v

proc clone*(v: CBSeq): CBSeq {.inline.} =
  initSeq(result)
  for item in v.mitems:
    result.push item.clone()

