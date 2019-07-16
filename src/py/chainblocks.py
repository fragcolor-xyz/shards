from cblocks import *
from cbtypes import *
from cbchain import*

def pycall(input):
  print("Yep.. worked!")
  return (CBType.String, "Yep.. worked indeed...")

@chain
def MainChain():
  Const(value = (CBType.String, "Hello world!"))
  Log()
  Const(value = (CBType.Float4, (1.0, 2.0, 3.0, 4.0)))
  Math.Sqrt()
  Log()
  Py(closure = (CBType.Object, (pycall, 1734439526, 2035317104)))
  Py(closure = (CBType.Object, (pycall, 1734439526, 2035317104)))
  Py(closure = (CBType.Object, (pycall, 1734439526, 2035317104)))
  Log()

mainChain = MainChain()
mainChain.start(True)

for _ in range(10):
  mainChain.tick()
  Chain.sleep(0.1)
