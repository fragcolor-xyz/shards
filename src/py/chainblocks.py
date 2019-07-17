from chainblocks import *

def pycall(state, input):
  print("Yep.. worked!")
  return cbvar("Yep.. worked indeed...")

@chain
def MainChain():
  Const((2, 3))
  Log()
  Const(value = "Hello world!")
  Log()
  Const(value = (1.0, 2.0, 3.0, 4.0))
  Math.Sqrt()
  Log()
  Py(closure = pycall)
  Py(closure = pycall)
  Py(closure = pycall)
  Log()
  Const(cbcolor(100, 200, 200, 255))
  Log()

mainChain = MainChain()
mainChain.start(True)

for _ in range(10):
  mainChain.tick()
  cbsleep(0.1)
