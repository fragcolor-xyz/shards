from enum import IntEnum, auto
from .chainblocks import *

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
  Color = auto()
  Chain = auto()
  Block = auto()
  EndOfBlittableTypes = auto()
  String = auto()
  ContextVar = auto()
  Image = auto()
  Seq = auto()
  Table = auto()

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
    elif type(value) is CBRuntimeBlock:
      self.value = (CBType.Block, value.block)
    elif type(value) is list:
      self.value = (CBType.Seq, [CBVar(x) for x in value])
    elif callable(value):
      self.value = (CBType.Object, (value, 1734439526, 2035317104))
    elif type(value) is tuple:
      if type(value[0]) is CBType:
        # e.g. cbcolor!
        self.value = value
      elif type(value[0]) is CBRuntimeBlock:
        self.value = (CBType.Seq, [CBVar(x).value for x in list(value)])
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
      raise Exception("CBVar value not handled! " + str(type(value)))

def cbvar(value):
  return CBVar(value).value

def cbcolor(v1, v2, v3, v4):
  assert(type(v1) is int and type(v2) is int and type(v3) is int and type(v4) is int)
  return (CBType.Color, (v1, v2, v3, v4))

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

# TODO rules
# expose/consume, make sure there is only 1 expose, any overwrite to expose should trigger error, we might want to have parameters for those tho to allow it?

class CBRuntimeBlock:
  def __init__(self, nativeBlock):
    self.block = nativeBlock

class CBChain:
  def __init__(self, name, **args):
    self.chain = createChain(name)
    self.looped = False
    self.unsafe = False
    
    if args.get("looped"):
      self.looped = True
    
    if args.get("unsafe"):
      self.unsafe = True
    
    blocks = args.get("blocks")
    if blocks == None:
      raise Exception("blocks missing from chain declaration!")
    previousBlock = None
    for block in blocks:
      if previousBlock != None:
        # Validate connection
        prevOutInfo = unpackTypesInfo(blockOutputTypes(previousBlock.block))
        currInInfo = unpackTypesInfo(blockInputTypes(block.block))
        if not validateConnection(prevOutInfo, currInInfo):
          raise Exception("Blocks do not connect, output: {}, input: {}".format(
            blockName(previousBlock.block), 
            blockName(block.block)))
        # Validate expose/consume
        # TODO
      
      # Finally add to the chain and continue
      chainAddBlock(self.chain, block.block)
      previousBlock = block

class CBNode:
  def __init__(self):
    self.node = createNode()
  
  def schedule(self, chain, inputVar = None):
    if inputVar == None:
      nodeSchedule(self.node, chain.chain, (0, None), chain.looped, chain.unsafe)
    else:
      nodeSchedule(self.node, chain.chain, inputVar, chain.looped, chain.unsafe)

  def tick(self, inputVar = None):
    if inputVar != None:
      nodeTick2(self.node, inputVar)
    else:
      nodeTick(self.node)
  
  def stop(self):
    nodeStop(self.node)

def cbsleep(seconds):
  chainSleep(seconds)
