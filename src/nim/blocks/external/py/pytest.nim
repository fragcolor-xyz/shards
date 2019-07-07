import ../../../chainblocks
import dynlib, os

putEnv("PYTHONHOME", "C:/ProgramData/Miniconda3")
putEnv("PYTHONPATH", "C:/ProgramData/Miniconda3")

proc run() =
  var lib = loadLib("py")
  assert lib != nil
  var libInit = cast[proc(registry: pointer) {.cdecl.}](lib.symAddr("chainblocks_init_module"))
  libInit(getGlobalRegistry())

  var chain = initChain()
  setCurrentChain(chain)

  Const "Hello"
  var pyBlk = createBlock("Scripting.Py")
  assert pyBlk != nil
  assert pyBlk.name(pyBlk) == "Scripting.Py", "Got: " & pyBlk.name(pyBlk)
  pyBlk.setup(pyBlk)
  pyBlk.setParam(pyBlk, 0, "./pytest.py")
  getCurrentChain().add(pyBlk)
  Log()
  
  echo "Start"
  var frame = getFrameState()
  getCurrentChain().start(true)
  setFrameState(frame)

  echo "First tick"
  frame = getFrameState()
  getCurrentChain().tick()
  setFrameState(frame)
  
  echo "Stop"
  frame = getFrameState()
  discard getCurrentChain().stop()
  setFrameState(frame)
  
  echo "Start again"
  frame = getFrameState()
  getCurrentChain().start(true)
  setFrameState(frame)

  while true:
    frame = getFrameState()
    getCurrentChain().tick()
    setFrameState(frame)
    sleep 30
  
run()