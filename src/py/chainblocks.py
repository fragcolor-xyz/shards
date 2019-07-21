from chainblocks import *
import inspect

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
  Repeat(times = 5, action = Msg("Hello repeating..."))
  print(dir())

node = CBNode()
mainChain = MainChain()
node.schedule(mainChain, looped = True)

for _ in range(10):
  node.tick()
  cbsleep(0.1)

node.stop()
