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
  
  log "Start"
  getCurrentChain().start(true)

  log "First tick"
  getCurrentChain().tick()
  
  log "Stop"
  getCurrentChain().stop()
  
  log "Start again"
  getCurrentChain().start(true)

  for _ in 0..100:
    getCurrentChain().tick()
    sleep 30
  
run()