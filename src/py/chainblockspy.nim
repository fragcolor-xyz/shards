import nimpy
import nimpy/[py_lib, py_types]
import ../nim/chainblocks
import nimline
import dynlib

pyExportModuleName("chainblocks")

proc cbCreateBlock*(name: cstring): ptr CBRuntimeBlock {.cdecl, importc, dynlib: "chainblocks".}
proc cbGetCurrentChain*(): CBChainPtr {.cdecl, importc, dynlib: "chainblocks".}
proc cbSetCurrentChain*(chain: CBChainPtr) {.cdecl, importc, dynlib: "chainblocks".}
proc cbRegisterChain*(chain: CBChainPtr) {.cdecl, importc, dynlib: "chainblocks".}
proc cbUnregisterChain*(chain: CBChainPtr) {.cdecl, importc, dynlib: "chainblocks".}
proc cbAddBlock*(chain: CBChainPtr; blk: ptr CBRuntimeBlock) {.cdecl, importc, dynlib: "chainblocks".}
proc cbStart*(chain: CBChainPtr; loop: cint; unsafe: cint) {.cdecl, importc, dynlib: "chainblocks".}
proc cbStop*(chain: CBChainPtr): CBVar {.cdecl, importc, dynlib: "chainblocks".}
proc cbCreateChain*(name: cstring): CBChainPtr {.cdecl, importc, dynlib: "chainblocks".}
proc cbDestroyChain*(chain: CBChainPtr) {.cdecl, importc, dynlib: "chainblocks".}
proc cbSleep*(secs: float64) {.cdecl, importc, dynlib: "chainblocks".}
proc cbBlocks*(): CBStrings {.cdecl, importc, dynlib: "chainblocks".}
proc cbFreeSeq*(seqp: pointer) {.cdecl, importc, dynlib: "chainblocks".}

proc getCurrentChain(): PPyObject {.exportpy.} =
  let current = cbGetCurrentChain()
  if current != nil:
    return py_lib.pyLib.PyCapsule_New(cast[pointer](current), nil, nil)
  else:
    return newPyNone()

proc setCurrentChain(chain: PPyObject) {.exportpy.} =
  let p = py_lib.pyLib.PyCapsule_GetPointer(chain, nil)
  cbSetCurrentChain(cast[CBChainPtr](p))

proc newChain(name: string): PPyObject {.exportpy.} =
  py_lib.pyLib.PyCapsule_New(cast[pointer](cbCreateChain(name)), nil, nil)

proc createBlock(name: string): PPyObject {.exportpy.} =
  py_lib.pyLib.PyCapsule_New(cast[pointer](cbCreateBlock(name)), nil, nil)

proc blocks(): seq[string] {.exportpy.} =
  var blocksSeq = cbBlocks()
  for blockName in blocksSeq.mitems:
    result.add($blockName)
  cbFreeSeq(blocksSeq)

proc addBlock(chain, blk: PPyObject) {.exportpy.} =
  if not chain.isNil and not blk.isNil:
    let
      p1 = py_lib.pyLib.PyCapsule_GetPointer(chain, nil)
      p2 = py_lib.pyLib.PyCapsule_GetPointer(blk, nil)
    var
      nchain = cast[CBChainPtr](p1)
      nblk = cast[ptr CBRuntimeBlock](p2)
    nchain.cbAddBlock(nblk)

proc start(chain: PPyObject; looped: bool; unsafe: bool) {.exportpy.} =
  let p = py_lib.pyLib.PyCapsule_GetPointer(chain, nil)
  cbStart(cast[CBChainPtr](p), looped.cint, unsafe.cint)
