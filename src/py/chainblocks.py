from chainblocks import *

@cbcall
def pycall(state, input):
  print("Yep.. worked!")
  return cbstring("Yep.. worked indeed...")

@chain
def MainChain():
  Const(cbint2(2, 3))
  Log()
  Const(value = cbstring("Hello world!"))
  Log()
  Const(value = cbfloat4(1.0, 2.0, 3.0, 4.0))
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
  Chain.sleep(0.1)
