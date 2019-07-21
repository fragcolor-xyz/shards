# This file was automatically generated.

import chainblocks
from .cbcore import *

def GetItems(index = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("GetItems"))
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: GetItems")
  chainblocks.blockSetup(blk.block)
  if index != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(index).value)
  if chainblocks.getCurrentChain() != None:
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
  setPreviousBlock(blk)
  return blk

def ToImage(width = None, height = None, channels = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("ToImage"))
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: ToImage")
  chainblocks.blockSetup(blk.block)
  if width != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(width).value)
  if height != None:
    chainblocks.blockSetParam(blk.block, 1, CBVar(height).value)
  if channels != None:
    chainblocks.blockSetParam(blk.block, 2, CBVar(channels).value)
  if chainblocks.getCurrentChain() != None:
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
  setPreviousBlock(blk)
  return blk

def Const(value = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("Const"))
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Const")
  chainblocks.blockSetup(blk.block)
  if value != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(value).value)
  if chainblocks.getCurrentChain() != None:
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
  setPreviousBlock(blk)
  return blk

def SetGlobal(name = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("SetGlobal"))
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: SetGlobal")
  chainblocks.blockSetup(blk.block)
  if name != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(name).value)
  if chainblocks.getCurrentChain() != None:
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
  setPreviousBlock(blk)
  return blk

def SwapVariables(name1 = None, name2 = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("SwapVariables"))
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: SwapVariables")
  chainblocks.blockSetup(blk.block)
  if name1 != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(name1).value)
  if name2 != None:
    chainblocks.blockSetParam(blk.block, 1, CBVar(name2).value)
  if chainblocks.getCurrentChain() != None:
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
  setPreviousBlock(blk)
  return blk

def WhenNot(reject = None, isregex = None, all = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("WhenNot"))
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: WhenNot")
  chainblocks.blockSetup(blk.block)
  if reject != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(reject).value)
  if isregex != None:
    chainblocks.blockSetParam(blk.block, 1, CBVar(isregex).value)
  if all != None:
    chainblocks.blockSetParam(blk.block, 2, CBVar(all).value)
  if chainblocks.getCurrentChain() != None:
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
  setPreviousBlock(blk)
  return blk

def If(operator = None, operand = None, true = None, false = None, passthrough = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("If"))
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: If")
  chainblocks.blockSetup(blk.block)
  if operator != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(operator).value)
  if operand != None:
    chainblocks.blockSetParam(blk.block, 1, CBVar(operand).value)
  if true != None:
    chainblocks.blockSetParam(blk.block, 2, CBVar(true).value)
  if false != None:
    chainblocks.blockSetParam(blk.block, 3, CBVar(false).value)
  if passthrough != None:
    chainblocks.blockSetParam(blk.block, 4, CBVar(passthrough).value)
  if chainblocks.getCurrentChain() != None:
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
  setPreviousBlock(blk)
  return blk

def ToString():
  blk = CBRuntimeBlock(chainblocks.createBlock("ToString"))
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: ToString")
  chainblocks.blockSetup(blk.block)
  if chainblocks.getCurrentChain() != None:
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
  setPreviousBlock(blk)
  return blk

def ToUppercase():
  blk = CBRuntimeBlock(chainblocks.createBlock("ToUppercase"))
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: ToUppercase")
  chainblocks.blockSetup(blk.block)
  if chainblocks.getCurrentChain() != None:
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
  setPreviousBlock(blk)
  return blk

def RunChain(chain = None, once = None, passthrough = None, detached = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("RunChain"))
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: RunChain")
  chainblocks.blockSetup(blk.block)
  if chain != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(chain).value)
  if once != None:
    chainblocks.blockSetParam(blk.block, 1, CBVar(once).value)
  if passthrough != None:
    chainblocks.blockSetParam(blk.block, 2, CBVar(passthrough).value)
  if detached != None:
    chainblocks.blockSetParam(blk.block, 3, CBVar(detached).value)
  if chainblocks.getCurrentChain() != None:
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
  setPreviousBlock(blk)
  return blk

def ChainStop():
  blk = CBRuntimeBlock(chainblocks.createBlock("ChainStop"))
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: ChainStop")
  chainblocks.blockSetup(blk.block)
  if chainblocks.getCurrentChain() != None:
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
  setPreviousBlock(blk)
  return blk

def ToLowercase():
  blk = CBRuntimeBlock(chainblocks.createBlock("ToLowercase"))
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: ToLowercase")
  chainblocks.blockSetup(blk.block)
  if chainblocks.getCurrentChain() != None:
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
  setPreviousBlock(blk)
  return blk

def GetVariable(name = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("GetVariable"))
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: GetVariable")
  chainblocks.blockSetup(blk.block)
  if name != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(name).value)
  if chainblocks.getCurrentChain() != None:
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
  setPreviousBlock(blk)
  return blk

def Py(closure = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("Py"))
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Py")
  chainblocks.blockSetup(blk.block)
  if closure != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(closure).value)
  if chainblocks.getCurrentChain() != None:
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
  setPreviousBlock(blk)
  return blk

def AddVariable(name = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("AddVariable"))
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: AddVariable")
  chainblocks.blockSetup(blk.block)
  if name != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(name).value)
  if chainblocks.getCurrentChain() != None:
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
  setPreviousBlock(blk)
  return blk

def Msg(message = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("Msg"))
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Msg")
  chainblocks.blockSetup(blk.block)
  if message != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(message).value)
  if chainblocks.getCurrentChain() != None:
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
  setPreviousBlock(blk)
  return blk

def WaitChain(chain = None, once = None, passthrough = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("WaitChain"))
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: WaitChain")
  chainblocks.blockSetup(blk.block)
  if chain != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(chain).value)
  if once != None:
    chainblocks.blockSetParam(blk.block, 1, CBVar(once).value)
  if passthrough != None:
    chainblocks.blockSetParam(blk.block, 2, CBVar(passthrough).value)
  if chainblocks.getCurrentChain() != None:
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
  setPreviousBlock(blk)
  return blk

def ChainRestart():
  blk = CBRuntimeBlock(chainblocks.createBlock("ChainRestart"))
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: ChainRestart")
  chainblocks.blockSetup(blk.block)
  if chainblocks.getCurrentChain() != None:
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
  setPreviousBlock(blk)
  return blk

def MatchText(regex = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("MatchText"))
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: MatchText")
  chainblocks.blockSetup(blk.block)
  if regex != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(regex).value)
  if chainblocks.getCurrentChain() != None:
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
  setPreviousBlock(blk)
  return blk

def Sleep(time = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("Sleep"))
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Sleep")
  chainblocks.blockSetup(blk.block)
  if time != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(time).value)
  if chainblocks.getCurrentChain() != None:
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
  setPreviousBlock(blk)
  return blk

def ToFloat():
  blk = CBRuntimeBlock(chainblocks.createBlock("ToFloat"))
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: ToFloat")
  chainblocks.blockSetup(blk.block)
  if chainblocks.getCurrentChain() != None:
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
  setPreviousBlock(blk)
  return blk

def ReplaceText(regex = None, replacement = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("ReplaceText"))
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: ReplaceText")
  chainblocks.blockSetup(blk.block)
  if regex != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(regex).value)
  if replacement != None:
    chainblocks.blockSetParam(blk.block, 1, CBVar(replacement).value)
  if chainblocks.getCurrentChain() != None:
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
  setPreviousBlock(blk)
  return blk

def SetVariable(name = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("SetVariable"))
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: SetVariable")
  chainblocks.blockSetup(blk.block)
  if name != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(name).value)
  if chainblocks.getCurrentChain() != None:
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
  setPreviousBlock(blk)
  return blk

def Log():
  blk = CBRuntimeBlock(chainblocks.createBlock("Log"))
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Log")
  chainblocks.blockSetup(blk.block)
  if chainblocks.getCurrentChain() != None:
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
  setPreviousBlock(blk)
  return blk

def WaitGlobal(name = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("WaitGlobal"))
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: WaitGlobal")
  chainblocks.blockSetup(blk.block)
  if name != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(name).value)
  if chainblocks.getCurrentChain() != None:
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
  setPreviousBlock(blk)
  return blk

def When(accept = None, isregex = None, all = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("When"))
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: When")
  chainblocks.blockSetup(blk.block)
  if accept != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(accept).value)
  if isregex != None:
    chainblocks.blockSetParam(blk.block, 1, CBVar(isregex).value)
  if all != None:
    chainblocks.blockSetParam(blk.block, 2, CBVar(all).value)
  if chainblocks.getCurrentChain() != None:
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
  setPreviousBlock(blk)
  return blk

def Repeat(forever = None, times = None, action = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("Repeat"))
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Repeat")
  chainblocks.blockSetup(blk.block)
  if forever != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(forever).value)
  if times != None:
    chainblocks.blockSetParam(blk.block, 1, CBVar(times).value)
  if action != None:
    chainblocks.blockSetParam(blk.block, 2, CBVar(action).value)
  if chainblocks.getCurrentChain() != None:
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
  setPreviousBlock(blk)
  return blk

class Math:
  def Multiply(operand = None):
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Multiply"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Multiply")
    chainblocks.blockSetup(blk.block)
    if operand != None:
      chainblocks.blockSetParam(blk.block, 0, CBVar(operand).value)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Asin():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Asin"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Asin")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def RShift(operand = None):
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.RShift"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.RShift")
    chainblocks.blockSetup(blk.block)
    if operand != None:
      chainblocks.blockSetParam(blk.block, 0, CBVar(operand).value)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Abs():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Abs"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Abs")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Log():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Log"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Log")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Sqrt():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Sqrt"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Sqrt")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Atanh():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Atanh"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Atanh")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Erf():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Erf"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Erf")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Round():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Round"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Round")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Mod(operand = None):
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Mod"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Mod")
    chainblocks.blockSetup(blk.block)
    if operand != None:
      chainblocks.blockSetParam(blk.block, 0, CBVar(operand).value)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Ceil():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Ceil"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Ceil")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Sin():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Sin"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Sin")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Xor(operand = None):
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Xor"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Xor")
    chainblocks.blockSetup(blk.block)
    if operand != None:
      chainblocks.blockSetParam(blk.block, 0, CBVar(operand).value)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def LGamma():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.LGamma"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.LGamma")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def And(operand = None):
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.And"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.And")
    chainblocks.blockSetup(blk.block)
    if operand != None:
      chainblocks.blockSetParam(blk.block, 0, CBVar(operand).value)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def LShift(operand = None):
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.LShift"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.LShift")
    chainblocks.blockSetup(blk.block)
    if operand != None:
      chainblocks.blockSetParam(blk.block, 0, CBVar(operand).value)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Cbrt():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Cbrt"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Cbrt")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Log1p():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Log1p"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Log1p")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Erfc():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Erfc"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Erfc")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Tan():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Tan"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Tan")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Acosh():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Acosh"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Acosh")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Expm1():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Expm1"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Expm1")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Or(operand = None):
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Or"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Or")
    chainblocks.blockSetup(blk.block)
    if operand != None:
      chainblocks.blockSetParam(blk.block, 0, CBVar(operand).value)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Atan():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Atan"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Atan")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Subtract(operand = None):
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Subtract"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Subtract")
    chainblocks.blockSetup(blk.block)
    if operand != None:
      chainblocks.blockSetParam(blk.block, 0, CBVar(operand).value)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Tanh():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Tanh"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Tanh")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Acos():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Acos"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Acos")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Asinh():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Asinh"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Asinh")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Add(operand = None):
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Add"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Add")
    chainblocks.blockSetup(blk.block)
    if operand != None:
      chainblocks.blockSetParam(blk.block, 0, CBVar(operand).value)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Cosh():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Cosh"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Cosh")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Exp():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Exp"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Exp")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Divide(operand = None):
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Divide"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Divide")
    chainblocks.blockSetup(blk.block)
    if operand != None:
      chainblocks.blockSetParam(blk.block, 0, CBVar(operand).value)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Trunc():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Trunc"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Trunc")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Floor():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Floor"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Floor")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Exp2():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Exp2"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Exp2")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Log2():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Log2"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Log2")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Sinh():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Sinh"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Sinh")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Cos():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Cos"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Cos")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Log10():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Log10"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Log10")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def TGamma():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.TGamma"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.TGamma")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
class Stack:
  def PopFloat2():
    blk = CBRuntimeBlock(chainblocks.createBlock("Stack.PopFloat2"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Stack.PopFloat2")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Peek(index = None):
    blk = CBRuntimeBlock(chainblocks.createBlock("Stack.Peek"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Stack.Peek")
    chainblocks.blockSetup(blk.block)
    if index != None:
      chainblocks.blockSetParam(blk.block, 0, CBVar(index).value)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def PopAdd():
    blk = CBRuntimeBlock(chainblocks.createBlock("Stack.PopAdd"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Stack.PopAdd")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Push():
    blk = CBRuntimeBlock(chainblocks.createBlock("Stack.Push"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Stack.Push")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def PopFloat4():
    blk = CBRuntimeBlock(chainblocks.createBlock("Stack.PopFloat4"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Stack.PopFloat4")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def PopFloat3():
    blk = CBRuntimeBlock(chainblocks.createBlock("Stack.PopFloat3"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Stack.PopFloat3")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Pop():
    blk = CBRuntimeBlock(chainblocks.createBlock("Stack.Pop"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Stack.Pop")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def PopInt3():
    blk = CBRuntimeBlock(chainblocks.createBlock("Stack.PopInt3"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Stack.PopInt3")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def PopDivide():
    blk = CBRuntimeBlock(chainblocks.createBlock("Stack.PopDivide"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Stack.PopDivide")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def PopMultiply():
    blk = CBRuntimeBlock(chainblocks.createBlock("Stack.PopMultiply"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Stack.PopMultiply")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def PopInt4():
    blk = CBRuntimeBlock(chainblocks.createBlock("Stack.PopInt4"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Stack.PopInt4")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def PopInt2():
    blk = CBRuntimeBlock(chainblocks.createBlock("Stack.PopInt2"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Stack.PopInt2")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def PushUnpacked():
    blk = CBRuntimeBlock(chainblocks.createBlock("Stack.PushUnpacked"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Stack.PushUnpacked")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def PopSubtract():
    blk = CBRuntimeBlock(chainblocks.createBlock("Stack.PopSubtract"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Stack.PopSubtract")
    chainblocks.blockSetup(blk.block)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
class IPC:
  def Push(name = None):
    blk = CBRuntimeBlock(chainblocks.createBlock("IPC.Push"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: IPC.Push")
    chainblocks.blockSetup(blk.block)
    if name != None:
      chainblocks.blockSetParam(blk.block, 0, CBVar(name).value)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
  def Pop(name = None):
    blk = CBRuntimeBlock(chainblocks.createBlock("IPC.Pop"))
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock().block))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk.block))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: IPC.Pop")
    chainblocks.blockSetup(blk.block)
    if name != None:
      chainblocks.blockSetParam(blk.block, 0, CBVar(name).value)
    if chainblocks.getCurrentChain() != None:
      chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk.block)
    setPreviousBlock(blk)
    return blk
  
