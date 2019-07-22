import nimpy
import nimpy/[py_lib, py_types]
import ../nim/chainblocks
import nimline
import tables, sets, dynlib
import varspy
import fragments/serialization

var
  inputSeqCache: seq[PPyObject]
  inputTableCache = initTable[string, PPyObject]()
  stringStorage: string
  stringsStorage: seq[string]
  seqStorage: CBSeq
  tableStorage: CBTable
  outputTableKeyCache = initHashSet[cstring]()

initSeq(seqStorage)
initTable(tableStorage)

pyExportModuleName("chainblocks")

let
  py = pyBuiltinsModule()

proc cbCreateBlock*(name: cstring): ptr CBRuntimeBlock {.cdecl, importc, dynlib: "chainblocks".}
proc cbBlocks*(): CBStrings {.cdecl, importc, dynlib: "chainblocks".}

proc cbGetCurrentChain*(): CBChainPtr {.cdecl, importc, dynlib: "chainblocks".}
proc cbSetCurrentChain*(chain: CBChainPtr) {.cdecl, importc, dynlib: "chainblocks".}

proc cbStart*(chain: CBChainPtr; loop: cint; unsafe: cint; input: CBVar) {.cdecl, importc, dynlib: "chainblocks".}
proc cbTick*(chain: CBChainPtr; rootInput: CBVar) {.cdecl, importc, dynlib: "chainblocks".}
proc cbSleep*(secs: float64) {.cdecl, importc, dynlib: "chainblocks".}
proc cbStop*(chain: CBChainPtr): CBVar {.cdecl, importc, dynlib: "chainblocks".}

proc cbCreateChain*(name: cstring): CBChainPtr {.cdecl, importc, dynlib: "chainblocks".}
proc cbDestroyChain*(chain: CBChainPtr) {.cdecl, importc, dynlib: "chainblocks".}
proc cbAddBlock*(chain: CBChainPtr; blk: ptr CBRuntimeBlock) {.cdecl, importc, dynlib: "chainblocks".}

proc cbCreateNode*(): ptr CBNode {.cdecl, importc, dynlib: "chainblocks".}
proc cbDestroyNode*(node: ptr CBNode) {.cdecl, importc, dynlib: "chainblocks".}
proc cbSchedule*(node: ptr CBNode, chain: CBChainPtr; input: CBVar; loop: cint; unsafe: cint) {.cdecl, importc, dynlib: "chainblocks".}
proc cbNodeTick*(node: ptr CBNode, input: CBVar) {.cdecl, importc, dynlib: "chainblocks".}
proc cbNodeStop*(node: ptr CBNode) {.cdecl, importc, dynlib: "chainblocks".}

# Import any other blocks dll
# let fragPrivateBlocks {.used.} = loadLib("fragblocks")

# Py - python interop
when true:
  const
    PyCallCC* = toFourCC("pyPy")
  
  let
    PyCallInfo* = CBTypeInfo(basicType: Object, objectVendorId: FragCC.int32, objectTypeId: PyCallCC.int32)

  static:
    echo FragCC.int32, " ", PyCallCC.int32
  
  # DON'T Register it actually
  # registerObjectType(FragCC, PyCallCC, CBObjectInfo(name: "Python interop"))

  type
    CBPyCallObj = object
      call: proc(state: PyObject, input: PPyObject): PyObject {.closure.}
    
    CBPyCallRef = ref CBPyCallObj

    CBPyCall* = object
      call: CBVar
      callObj: CBPyCallRef
      state: PyObject
      inputSeqCache: seq[PPyObject]
      inputTableCache: Table[string, PPyObject]
      stringStorage: string
      stringsStorage: seq[string]
      seqStorage: CBSeq
      tableStorage: CBTable
      outputTableKeyCache: HashSet[cstring]
      currentResult: PyObject
  
  template setup*(b: CBPyCall) =
    initSeq(seqStorage)
    initTable(tableStorage)
    b.inputTableCache = initTable[string, PPyObject]()
    b.outputTableKeyCache = initHashSet[cstring]()
    b.state = py.dict()
  template destroy*(b: CBPyCall) =
    freeSeq(seqStorage)
    freeTable(tableStorage)
  template cleanup*(b: CBPyCall) =
    b.state = py.dict()
  template inputTypes*(b: CBPyCall): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBPyCall): CBTypesInfo = ({ Any }, true #[seq]#)
  template parameters*(b: CBPyCall): CBParametersInfo = @[("Closure", @[PyCallInfo])]
  template setParam*(b: CBPyCall; index: int; val: CBVar) =
    b.call = val
    var callObj = cast[ref CBPyCallObj](val.objectValue)
    b.callObj = callObj
  template getParam*(b: CBPyCall; index: int): CBVar = b.call
  template activate*(b: CBPyCall; context: CBContext; input: CBVar): CBVar =
    b.currentResult = b.callObj.call(b.state, var2Py(input, b.inputSeqCache, b.inputTableCache))
    py2Var(b.currentResult, b.stringStorage, b.stringsStorage, b.seqStorage, b.tableStorage, b.outputTableKeyCache)

  chainblock CBPyCall, "Py"

proc getCurrentChain(): PPyObject {.exportpy.} =
  let current = cbGetCurrentChain()
  if current != nil:
    return py_lib.pyLib.PyCapsule_New(cast[pointer](current), nil, nil)
  else:
    return newPyNone()

proc setCurrentChain(chain: PPyObject) {.exportpy.} =
  if chain.pointer == newPyNone().pointer:
    cbSetCurrentChain(nil)
  else:
    let p = py_lib.pyLib.PyCapsule_GetPointer(chain, nil)
    cbSetCurrentChain(cast[CBChainPtr](p))

proc pyChainDestroy(chain: PPyObject) {.cdecl.} =
  let p = py_lib.pyLib.PyCapsule_GetPointer(chain, nil)
  cbDestroyChain(cast[CBChainPtr](p))

proc createChain(name: string): PPyObject {.exportpy.} =
  py_lib.pyLib.PyCapsule_New(cast[pointer](cbCreateChain(name)), nil, pyChainDestroy)

proc pyNodeDestroy(node: PPyObject) {.cdecl.} =
  let p = py_lib.pyLib.PyCapsule_GetPointer(node, nil)
  cbDestroyNode(cast[ptr CBNode](p))

proc createNode(): PPyObject {.exportpy.} =
  py_lib.pyLib.PyCapsule_New(cast[pointer](cbCreateNode()), nil, pyNodeDestroy)

proc createBlock(name: string): PPyObject {.exportpy.} =
  py_lib.pyLib.PyCapsule_New(cast[pointer](cbCreateBlock(name)), nil, nil)

proc blocks(): seq[string] {.exportpy.} =
  var blocksSeq = cbBlocks()
  for blockName in blocksSeq.mitems:
    result.add($blockName)

proc chainSleep(seconds: float64) {.exportpy.} = cbSleep(seconds)

proc chainAddBlock(chain, blk: PPyObject) {.exportpy.} =
  if not chain.isNil and not blk.isNil:
    let
      p1 = py_lib.pyLib.PyCapsule_GetPointer(chain, nil)
      p2 = py_lib.pyLib.PyCapsule_GetPointer(blk, nil)
    var
      nchain = cast[CBChainPtr](p1)
      nblk = cast[ptr CBRuntimeBlock](p2)
    nchain.cbAddBlock(nblk)

proc chainStart(chain: PPyObject; looped: bool; unsafe: bool) {.exportpy.} =
  let p = py_lib.pyLib.PyCapsule_GetPointer(chain, nil)
  cbStart(cast[CBChainPtr](p), looped.cint, unsafe.cint, Empty)

proc chainStart2(chain: PPyObject; looped: bool; unsafe: bool; input: PyObject) {.exportpy.} =
  let p = py_lib.pyLib.PyCapsule_GetPointer(chain, nil)
  cbStart(cast[CBChainPtr](p), looped.cint, unsafe.cint, py2Var(input, stringStorage, stringsStorage, seqStorage, tableStorage, outputTableKeyCache))

proc chainTick(chain: PPyObject; input: PyObject) {.exportpy.} =
  let p = py_lib.pyLib.PyCapsule_GetPointer(chain, nil)
  var value = py2Var(input, stringStorage, stringsStorage, seqStorage, tableStorage, outputTableKeyCache)
  cbTick(cast[CBChainPtr](p), value)

proc chainTick2(chain: PPyObject) {.exportpy.} =
  let p = py_lib.pyLib.PyCapsule_GetPointer(chain, nil)
  cbTick(cast[CBChainPtr](p), Empty)

proc chainStop(chain: PPyObject): PPyObject {.exportpy.} =
  let p = py_lib.pyLib.PyCapsule_GetPointer(chain, nil)
  var2Py(cbStop(cast[CBChainPtr](p)), inputSeqCache, inputTableCache)

proc chainStop2(chain: PPyObject) {.exportpy.} =
  let p = py_lib.pyLib.PyCapsule_GetPointer(chain, nil)
  discard cbStop(cast[CBChainPtr](p))

proc nodeSchedule(node, chain: PPyObject; input: PyObject; looped, unsafe: bool) {.exportpy.} =
  let
    pnode = cast[ptr CBNode](py_lib.pyLib.PyCapsule_GetPointer(node, nil))
    pchain = cast[CBChainPtr](py_lib.pyLib.PyCapsule_GetPointer(chain, nil))
    value = py2Var(input, stringStorage, stringsStorage, seqStorage, tableStorage, outputTableKeyCache)
  cbSchedule(pnode, pchain, value, looped.cint, unsafe.cint)

proc nodeTick(node: PPyObject) {.exportpy.} =
  let
    pnode = cast[ptr CBNode](py_lib.pyLib.PyCapsule_GetPointer(node, nil))
    value = Empty
  cbNodeTick(pnode, value)

proc nodeTick2(node: PPyObject; input: PyObject) {.exportpy.} =
  let
    pnode = cast[ptr CBNode](py_lib.pyLib.PyCapsule_GetPointer(node, nil))
    value = py2Var(input, stringStorage, stringsStorage, seqStorage, tableStorage, outputTableKeyCache)
  cbNodeTick(pnode, value)

proc nodeStop(node: PPyObject) {.exportpy.} =
  let
    pnode = cast[ptr CBNode](py_lib.pyLib.PyCapsule_GetPointer(node, nil))
  cbNodeStop(pnode)

proc blockName(blk: PPyObject): string {.exportpy.} =
  let
    p = py_lib.pyLib.PyCapsule_GetPointer(blk, nil)
    cblk = cast[ptr CBRuntimeBlock](p)
  $cblk[].name(cblk)

proc blockHelp(blk: PPyObject): string {.exportpy.} =
  let
    p = py_lib.pyLib.PyCapsule_GetPointer(blk, nil)
    cblk = cast[ptr CBRuntimeBlock](p)
  $cblk[].help(cblk)

proc blockSetup(blk: PPyObject) {.exportpy.} =
  let
    p = py_lib.pyLib.PyCapsule_GetPointer(blk, nil)
    cblk = cast[ptr CBRuntimeBlock](p)
  cblk[].setup(cblk)

proc blockDestroy(blk: PPyObject) {.exportpy.} =
  let
    p = py_lib.pyLib.PyCapsule_GetPointer(blk, nil)
    cblk = cast[ptr CBRuntimeBlock](p)
  cblk[].destroy(cblk)

proc blockInputTypes(blk: PPyObject): PPyObject {.exportpy.} =
  let
    p = py_lib.pyLib.PyCapsule_GetPointer(blk, nil)
    cblk = cast[ptr CBRuntimeBlock](p)
  var payload = cblk[].inputTypes(cblk)
  return py_lib.pyLib.PyCapsule_New(cast[pointer](payload), nil, nil)

proc blockOutputTypes(blk: PPyObject): PPyObject {.exportpy.} =
  let
    p = py_lib.pyLib.PyCapsule_GetPointer(blk, nil)
    cblk = cast[ptr CBRuntimeBlock](p)
  var payload = cblk[].outputTypes(cblk)
  return py_lib.pyLib.PyCapsule_New(cast[pointer](payload), nil, nil)

proc blockExposedVariables(blk: PPyObject): PPyObject {.exportpy.} =
  let
    p = py_lib.pyLib.PyCapsule_GetPointer(blk, nil)
    cblk = cast[ptr CBRuntimeBlock](p)
  var payload = cblk[].exposedVariables(cblk)
  if payload != nil:
    return py_lib.pyLib.PyCapsule_New(cast[pointer](payload), nil, nil)
  else:
    return newPyNone()

proc blockConsumedVariables(blk: PPyObject): PPyObject {.exportpy.} =
  let
    p = py_lib.pyLib.PyCapsule_GetPointer(blk, nil)
    cblk = cast[ptr CBRuntimeBlock](p)
  var payload = cblk[].consumedVariables(cblk)
  if payload != nil:
    return py_lib.pyLib.PyCapsule_New(cast[pointer](payload), nil, nil)
  else:
    return newPyNone()

proc blockParameters(blk: PPyObject): PPyObject {.exportpy.} =
  let
    p = py_lib.pyLib.PyCapsule_GetPointer(blk, nil)
    cblk = cast[ptr CBRuntimeBlock](p)
  var payload = cblk[].parameters(cblk)
  if payload != nil:
    return py_lib.pyLib.PyCapsule_New(cast[pointer](payload), nil, nil)
  else:
    return newPyNone()

proc blockGetParam(blk: PPyObject; index: int): PPyObject {.exportpy.} =
  let
    p = py_lib.pyLib.PyCapsule_GetPointer(blk, nil)
    cblk = cast[ptr CBRuntimeBlock](p)
  var cbvar = cblk[].getParam(cblk, index)
  return var2Py(cbvar, inputSeqCache, inputTableCache)

proc blockSetParam(blk: PPyObject; index: int; val: PyObject) {.exportpy.} =
  seqStorage.clear()
  let
    p = py_lib.pyLib.PyCapsule_GetPointer(blk, nil)
    cblk = cast[ptr CBRuntimeBlock](p)
    name = cblk[].name(cblk)
  if name == "Py": # special case!
    var
      fullVal = val.to(tuple[valueType: int; value: PyObject])
    if fullVal.valueType.CBType == CBType.Object:
      var
        objValue = fullVal.value.to(tuple[closure: proc(state: PyObject, input: PPyObject): PyObject; vendor, typeid: int32])
      if objValue.vendor == FragCC.int32 and objValue.typeId == PyCallCC.int32:
        var callObj = new CBPyCallRef
        callObj[].call = objValue.closure
        cblk[].setParam(cblk, index, CBVar(valueType: Object, payload: CBVarPayload(objectVendorId: FragCC.int32, objectTypeId: PyCallCC.int32, objectValue: cast[pointer](callObj))))
  else:
    var value = py2Var(val, stringStorage, stringsStorage, seqStorage, tableStorage, outputTableKeyCache)
    cblk[].setParam(cblk, index, value)

proc unpackTypesInfo(tysInfo: PPyObject): seq[tuple[basicType: int; sequenced: bool]] {.exportpy.} =
  let
    p = py_lib.pyLib.PyCapsule_GetPointer(tysInfo, nil)
    ctys = cast[CBTypesInfo](p)
  for ty in ctys.mitems:
    result.add((ty.basicType.int, ty.sequenced))

proc unpackParametersInfo(paramsInfo: PPyObject): seq[tuple[name, help: string; valueTypes: PPyObject; allowContext: bool]] {.exportpy.} =
  let
    p = py_lib.pyLib.PyCapsule_GetPointer(paramsInfo, nil)
    psinfo = cast[CBParametersInfo](p)
  for pi in psinfo.mitems:
    result.add(($pi.name, $pi.help, py_lib.pyLib.PyCapsule_New(cast[pointer](pi.valueTypes), nil, nil), pi.allowContext))
