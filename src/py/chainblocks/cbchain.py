from .chainblocks import *

class CBChain:
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

def chain(func):
  def emitChain():
    previousChain = getCurrentChain()
    
    chain = createChain(func.__name__)
    setCurrentChain(chain)
    
    func()
    
    if previousChain != None:
      setCurrentChain(previousChain)
    
    return CBChain(chain)
  return emitChain

def cbsleep(seconds):
  chainSleep(seconds)