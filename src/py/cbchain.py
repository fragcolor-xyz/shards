from chainblocks import *

class Chain:
  def __init__(self, nativeChain):
    self.chain = nativeChain
  
  def start(self, looped = False, unsafe = False):
    chainStart(self.chain, looped, unsafe)
  
  def stop(self):
    return chainStop(self.chain)
  
  def tick(self, inputVar = None):
    if inputVar != None:
      chainTick(self.chain, inputVar)
    else:
      chainTick2(self.chain)

  def sleep(seconds):
    chainSleep(seconds)

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