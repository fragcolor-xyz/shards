from chainblocks import *

class Chain:
  def __init__(self, nativeChain):
    self.chain = nativeChain
  
  def start(self, looped = False, unsafe = False):
    chainStart(self.chain, looped, unsafe)
  
  def stop(self):
    return chainStop(self.chain)
  
  def tick(self):
    chainTick2(self.chain)
  
  def tick(self, inputVar):
    chainTick(self.chain, inputVar)


def chain(func):
  def emitChain():
    previousChain = getCurrentChain()
    
    chain = createChain(func.__name__)
    setCurrentChain(chain)
    
    func()
    
    if previousChain != None:
      setCurrentChain(previousChain)
    
    return Chain(chain)
  return emitChain