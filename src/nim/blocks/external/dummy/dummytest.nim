import ../../../chainblocks
import dynlib, os

proc run() =
  var lib = loadLib("dummy")
  assert lib != nil

  var chain = newChain("chain")
  setCurrentChain(chain)

  Const "Hello"

  var pyBlk = createBlock("Dummy.Dummy")
  assert pyBlk != nil
  assert $pyBlk.name(pyBlk) == "Dummy.Dummy", "Got: " & $pyBlk.name(pyBlk)
  pyBlk.setup(pyBlk)
  getCurrentChain().add(pyBlk)
  
  Log()
  
  echo "Start"
  getCurrentChain().start(true)

  echo "First tick"
  getCurrentChain().tick()
  
  echo "Stop"
  discard getCurrentChain().stop()
  
  echo "Start again"
  getCurrentChain().start(true)

  for _ in 0..100:
    getCurrentChain().tick()
    sleep 30
  
run()