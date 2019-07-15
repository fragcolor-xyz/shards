import nimpy
import nimpy/[py_lib, py_types]
import ../nim/chainblocks
import nimline

defineCppType(BlocksMap, "std::unordered_map<std::string, CBBlockConstructor>", "runtime.hpp")
defineCppType(BlocksPair, "std::pair<std::string, CBBlockConstructor>", "runtime.hpp")

var
  blocksRegister {.importcpp: "chainblocks::BlocksRegister", header: "runtime.hpp", nodecl.}: BlocksMap
  availableBlocks: seq[string]

for item in cppItems[BlocksMap, BlocksPair](blocksRegister):
  let itemName = item.first.c_str().to(cstring)
  availableBlocks.add($itemName)

for availblk in availableBlocks:
  echo "Available: ", availblk

proc getCurrentChain(): PPyObject {.exportpy.} =
  let current = chainblocks.getCurrentChain()
  if current != nil:
    return py_lib.pyLib.PyCapsule_New(cast[pointer](current), nil, nil)
  else:
    return newPyNone()

proc setCurrentChain(chain: PPyObject) {.exportpy.} =
  let p = py_lib.pyLib.PyCapsule_GetPointer(chain, nil)
  chainblocks.setCurrentChain(cast[CBChainPtr](p))

proc newChain(name: string): PPyObject {.exportpy.} =
  py_lib.pyLib.PyCapsule_New(cast[pointer](chainblocks.newChain(name)), nil, nil)

proc createBlock(name: string): PPyObject {.exportpy.} =
  py_lib.pyLib.PyCapsule_New(cast[pointer](chainblocks.createBlock(name)), nil, nil)

proc blocks(): seq[string] {.exportpy.} = availableBlocks

proc addBlock(chain, blk: PPyObject) {.exportpy.} =
  if not chain.isNil and not blk.isNil:
    let
      p1 = py_lib.pyLib.PyCapsule_GetPointer(chain, nil)
      p2 = py_lib.pyLib.PyCapsule_GetPointer(blk, nil)
    var
      nchain = cast[CBChainPtr](p1)
      nblk = cast[ptr CBRuntimeBlock](p2)
    nchain.add(nblk)

proc start(chain: PPyObject; looped: bool; unsafe: bool) {.exportpy.} =
  let p = py_lib.pyLib.PyCapsule_GetPointer(chain, nil)
  chainblocks.start(cast[CBChainPtr](p), looped, unsafe)
