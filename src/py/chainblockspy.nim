import nimpy
import nimpy/[py_lib, py_types]
import ../nim/chainblocks
import nimline
import dynlib, tables, sets
import varspy

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

proc cbCreateBlock*(name: cstring): ptr CBRuntimeBlock {.cdecl, importc, dynlib: "chainblocks".}
proc cbBlocks*(): CBStrings {.cdecl, importc, dynlib: "chainblocks".}

proc cbGetCurrentChain*(): CBChainPtr {.cdecl, importc, dynlib: "chainblocks".}
proc cbSetCurrentChain*(chain: CBChainPtr) {.cdecl, importc, dynlib: "chainblocks".}

proc cbStart*(chain: CBChainPtr; loop: cint; unsafe: cint) {.cdecl, importc, dynlib: "chainblocks".}
proc cbTick*(chain: CBChainPtr; rootInput: CBVar) {.cdecl, importc, dynlib: "chainblocks".}
proc cbSleep*(secs: float64) {.cdecl, importc, dynlib: "chainblocks".}
proc cbStop*(chain: CBChainPtr): CBVar {.cdecl, importc, dynlib: "chainblocks".}

proc cbCreateChain*(name: cstring): CBChainPtr {.cdecl, importc, dynlib: "chainblocks".}
proc cbDestroyChain*(chain: CBChainPtr) {.cdecl, importc, dynlib: "chainblocks".}
proc cbAddBlock*(chain: CBChainPtr; blk: ptr CBRuntimeBlock) {.cdecl, importc, dynlib: "chainblocks".}

proc getCurrentChain(): PPyObject {.exportpy.} =
  let current = cbGetCurrentChain()
  if current != nil:
    return py_lib.pyLib.PyCapsule_New(cast[pointer](current), nil, nil)
  else:
    return newPyNone()

proc setCurrentChain(chain: PPyObject) {.exportpy.} =
  let p = py_lib.pyLib.PyCapsule_GetPointer(chain, nil)
  cbSetCurrentChain(cast[CBChainPtr](p))

proc createChain(name: string): PPyObject {.exportpy.} =
  py_lib.pyLib.PyCapsule_New(cast[pointer](cbCreateChain(name)), nil, nil)

proc createBlock(name: string): PPyObject {.exportpy.} =
  py_lib.pyLib.PyCapsule_New(cast[pointer](cbCreateBlock(name)), nil, nil)

proc blocks(): seq[string] {.exportpy.} =
  var blocksSeq = cbBlocks()
  for blockName in blocksSeq.mitems:
    result.add($blockName)

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
  cbStart(cast[CBChainPtr](p), looped.cint, unsafe.cint)

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
    value = py2Var(val, stringStorage, stringsStorage, seqStorage, tableStorage, outputTableKeyCache)
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
