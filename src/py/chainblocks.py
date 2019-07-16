from cblocks import *
from cbtypes import *
from cbchain import*

@chain
def MainChain():
  Const(value = (CBType.String, "Hello world!"))
  Log()
  Const(value = (CBType.Float4, (1.0, 2.0, 3.0, 4.0)))
  Math.Sqrt()
  Log()

mainChain = MainChain()
mainChain.start()
