from enum import IntEnum, auto
from .chainblocks import *

class CBChain:
  def __init__(self, nativeChain):
    self.chain = nativeChain
  
  def start(self, looped = False, unsafe = False, inputVar = None):
    if inputVar != None:
      chainStart2(self.chain, looped, unsafe, CBVar(inputVar).value)
    else:
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

class CBType(IntEnum):
  NoType = 0
  Any = auto()
  Object = auto()
  Enum = auto()
  Bool = auto()
  Int = auto()
  Int2 = auto()
  Int3 = auto()
  Int4 = auto()
  Int8 = auto()
  Int16 = auto()
  Float = auto()
  Float2 = auto()
  Float3 = auto()
  Float4 = auto()
  String = auto()
  Color = auto()
  Image = auto()
  Seq = auto()
  Table = auto()
  Chain = auto()
  Block = auto()
  ContextVar = auto()

def validateConnection(outputInfo, inputInfo):
  for iinfo in inputInfo:
    if iinfo[0] == CBType.Any:
      return True
    else:
      for oinfo in outputInfo:
        if oinfo[0] == CBType.Any:
          return True
        else:
          if iinfo[0] == oinfo[0] and iinfo[1] == oinfo[1]: # Types match, also sequenced
            return True

class CBVar:
  def __init__(self, value):
    if value == None:
      self.value = (CBType.NoType, None)
    elif type(value) is bool:
      self.value = (CBType.Bool, value)
    elif type(value) is int:
      self.value = (CBType.Int, value)
    elif type(value) is float:
      self.value = (CBType.Float, value)
    elif type(value) is str:
      self.value = (CBType.String, value)
    elif type(value) is CBChain:
      self.value = (CBType.Chain, value.chain)
    elif callable(value):
      self.value = (CBType.Object, (value, 1734439526, 2035317104))
    elif type(value) is tuple:
      if type(value[0]) is CBType:
        # e.g. cbcolor!
        self.value = value
      else:
        hasFloats = False
        valueLen = len(value)
        for val in value:
          valType = type(val)
          if valType is float:
            hasFloats = True
          elif valType is not int:
            raise Exception("CBVar from tuple must be int or float")
        
        if hasFloats and valueLen > 4:
          raise Exception("CBVar from a tuple of float values max length is 4")
        elif valueLen > 16:
          raise Exception("CBVar from a tuple of int values max length is 16")
        
        if hasFloats and valueLen == 1:
          self.value = (CBType.Float, value[0])
        elif hasFloats and valueLen == 2:
          self.value = (CBType.Float2, value)
        elif hasFloats and valueLen == 3:
          self.value = (CBType.Float3, value)
        elif hasFloats and valueLen == 4:
          self.value = (CBType.Float4, value)
        elif valueLen == 1:
          self.value = (CBType.Int, value[0])
        elif valueLen == 2:
          self.value = (CBType.Int2, value)
        elif valueLen == 3:
          self.value = (CBType.Int3, value)
        elif valueLen == 4:
          self.value = (CBType.Int4, value)
        elif valueLen == 8:
          self.value = (CBType.Int8, value)
        elif valueLen == 16:
          self.value = (CBType.Int16, value)
        else:
          raise Exception("CBVar from a tuple has an invalid length, must be 1, 2, 3, 4, 8 (int only), 16 (int only)")

    else:
      raise Exception("CBVar value not handled!")

def cbvar(value):
  return CBVar(value).value

def cbcolor(v1, v2, v3, v4):
  assert(type(v1) is int and type(v2) is int and type(v3) is int and type(v4) is int)
  return (CBType.Color, (v1, v2, v3, v4))