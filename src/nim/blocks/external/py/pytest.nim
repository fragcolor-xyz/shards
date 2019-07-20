import ../../../chainblocks
import dynlib, os

putEnv("PYTHONHOME", "C:/ProgramData/Miniconda3")
putEnv("PYTHONPATH", "C:/ProgramData/Miniconda3")

proc run() =
  var lib = loadLib("py")
  assert lib != nil

  var chain = newChain("chain")
  setCurrentChain(chain)

  Const "Hello"

  var pyBlk = createBlock("Scripting.Py")
  assert pyBlk != nil
  assert $pyBlk.name(pyBlk) == "Scripting.Py", "Got: " & $pyBlk.name(pyBlk)
  pyBlk.setup(pyBlk)
  pyBlk.setParam(pyBlk, 0, "./pytest.py")
  getCurrentChain().add(pyBlk)
  
  Log()
  
  echo "Start"
  getCurrentChain().start(true)

  echo "First tick"
  getCurrentChain().tick()
  
  echo "Stop"
  getCurrentChain().stop()
  
  echo "Start again"
  getCurrentChain().start(true)

  for _ in 0..100:
    getCurrentChain().tick()
    sleep 30
  
run()
GC_fullCollect()