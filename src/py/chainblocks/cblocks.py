# This file was automatically generated.

import chainblocks
from .cbcore import *

def GetItems(index = None):
  blk = chainblocks.createBlock("GetItems")
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: GetItems")
  chainblocks.blockSetup(blk)
  if index != None:
    chainblocks.blockSetParam(blk, 0, CBVar(index).value)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  setPreviousBlock(blk)

def ToImage(width = None, height = None, channels = None):
  blk = chainblocks.createBlock("ToImage")
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: ToImage")
  chainblocks.blockSetup(blk)
  if width != None:
    chainblocks.blockSetParam(blk, 0, CBVar(width).value)
  if height != None:
    chainblocks.blockSetParam(blk, 1, CBVar(height).value)
  if channels != None:
    chainblocks.blockSetParam(blk, 2, CBVar(channels).value)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  setPreviousBlock(blk)

def Const(value = None):
  blk = chainblocks.createBlock("Const")
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Const")
  chainblocks.blockSetup(blk)
  if value != None:
    chainblocks.blockSetParam(blk, 0, CBVar(value).value)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  setPreviousBlock(blk)

def SetGlobal(name = None):
  blk = chainblocks.createBlock("SetGlobal")
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: SetGlobal")
  chainblocks.blockSetup(blk)
  if name != None:
    chainblocks.blockSetParam(blk, 0, CBVar(name).value)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  setPreviousBlock(blk)

def SwapVariables(name1 = None, name2 = None):
  blk = chainblocks.createBlock("SwapVariables")
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: SwapVariables")
  chainblocks.blockSetup(blk)
  if name1 != None:
    chainblocks.blockSetParam(blk, 0, CBVar(name1).value)
  if name2 != None:
    chainblocks.blockSetParam(blk, 1, CBVar(name2).value)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  setPreviousBlock(blk)

def WhenNot(reject = None, isregex = None, all = None):
  blk = chainblocks.createBlock("WhenNot")
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: WhenNot")
  chainblocks.blockSetup(blk)
  if reject != None:
    chainblocks.blockSetParam(blk, 0, CBVar(reject).value)
  if isregex != None:
    chainblocks.blockSetParam(blk, 1, CBVar(isregex).value)
  if all != None:
    chainblocks.blockSetParam(blk, 2, CBVar(all).value)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  setPreviousBlock(blk)

def If(operator = None, operand = None, true = None, false = None, passthrough = None):
  blk = chainblocks.createBlock("If")
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: If")
  chainblocks.blockSetup(blk)
  if operator != None:
    chainblocks.blockSetParam(blk, 0, CBVar(operator).value)
  if operand != None:
    chainblocks.blockSetParam(blk, 1, CBVar(operand).value)
  if true != None:
    chainblocks.blockSetParam(blk, 2, CBVar(true).value)
  if false != None:
    chainblocks.blockSetParam(blk, 3, CBVar(false).value)
  if passthrough != None:
    chainblocks.blockSetParam(blk, 4, CBVar(passthrough).value)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  setPreviousBlock(blk)

def ToString():
  blk = chainblocks.createBlock("ToString")
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: ToString")
  chainblocks.blockSetup(blk)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  setPreviousBlock(blk)

def ToUppercase():
  blk = chainblocks.createBlock("ToUppercase")
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: ToUppercase")
  chainblocks.blockSetup(blk)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  setPreviousBlock(blk)

def RunChain(chain = None, once = None, passthrough = None, detached = None):
  blk = chainblocks.createBlock("RunChain")
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: RunChain")
  chainblocks.blockSetup(blk)
  if chain != None:
    chainblocks.blockSetParam(blk, 0, CBVar(chain).value)
  if once != None:
    chainblocks.blockSetParam(blk, 1, CBVar(once).value)
  if passthrough != None:
    chainblocks.blockSetParam(blk, 2, CBVar(passthrough).value)
  if detached != None:
    chainblocks.blockSetParam(blk, 3, CBVar(detached).value)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  setPreviousBlock(blk)

def ChainStop():
  blk = chainblocks.createBlock("ChainStop")
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: ChainStop")
  chainblocks.blockSetup(blk)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  setPreviousBlock(blk)

def ToLowercase():
  blk = chainblocks.createBlock("ToLowercase")
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: ToLowercase")
  chainblocks.blockSetup(blk)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  setPreviousBlock(blk)

def GetVariable(name = None):
  blk = chainblocks.createBlock("GetVariable")
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: GetVariable")
  chainblocks.blockSetup(blk)
  if name != None:
    chainblocks.blockSetParam(blk, 0, CBVar(name).value)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  setPreviousBlock(blk)

def Py(closure = None):
  blk = chainblocks.createBlock("Py")
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Py")
  chainblocks.blockSetup(blk)
  if closure != None:
    chainblocks.blockSetParam(blk, 0, CBVar(closure).value)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  setPreviousBlock(blk)

def AddVariable(name = None):
  blk = chainblocks.createBlock("AddVariable")
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: AddVariable")
  chainblocks.blockSetup(blk)
  if name != None:
    chainblocks.blockSetParam(blk, 0, CBVar(name).value)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  setPreviousBlock(blk)

def Msg(message = None):
  blk = chainblocks.createBlock("Msg")
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Msg")
  chainblocks.blockSetup(blk)
  if message != None:
    chainblocks.blockSetParam(blk, 0, CBVar(message).value)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  setPreviousBlock(blk)

def WaitChain(chain = None, once = None, passthrough = None):
  blk = chainblocks.createBlock("WaitChain")
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: WaitChain")
  chainblocks.blockSetup(blk)
  if chain != None:
    chainblocks.blockSetParam(blk, 0, CBVar(chain).value)
  if once != None:
    chainblocks.blockSetParam(blk, 1, CBVar(once).value)
  if passthrough != None:
    chainblocks.blockSetParam(blk, 2, CBVar(passthrough).value)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  setPreviousBlock(blk)

def ChainRestart():
  blk = chainblocks.createBlock("ChainRestart")
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: ChainRestart")
  chainblocks.blockSetup(blk)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  setPreviousBlock(blk)

def MatchText(regex = None):
  blk = chainblocks.createBlock("MatchText")
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: MatchText")
  chainblocks.blockSetup(blk)
  if regex != None:
    chainblocks.blockSetParam(blk, 0, CBVar(regex).value)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  setPreviousBlock(blk)

def Sleep(time = None):
  blk = chainblocks.createBlock("Sleep")
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Sleep")
  chainblocks.blockSetup(blk)
  if time != None:
    chainblocks.blockSetParam(blk, 0, CBVar(time).value)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  setPreviousBlock(blk)

def ToFloat():
  blk = chainblocks.createBlock("ToFloat")
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: ToFloat")
  chainblocks.blockSetup(blk)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  setPreviousBlock(blk)

def ReplaceText(regex = None, replacement = None):
  blk = chainblocks.createBlock("ReplaceText")
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: ReplaceText")
  chainblocks.blockSetup(blk)
  if regex != None:
    chainblocks.blockSetParam(blk, 0, CBVar(regex).value)
  if replacement != None:
    chainblocks.blockSetParam(blk, 1, CBVar(replacement).value)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  setPreviousBlock(blk)

def SetVariable(name = None):
  blk = chainblocks.createBlock("SetVariable")
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: SetVariable")
  chainblocks.blockSetup(blk)
  if name != None:
    chainblocks.blockSetParam(blk, 0, CBVar(name).value)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  setPreviousBlock(blk)

def Log():
  blk = chainblocks.createBlock("Log")
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Log")
  chainblocks.blockSetup(blk)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  setPreviousBlock(blk)

def WaitGlobal(name = None):
  blk = chainblocks.createBlock("WaitGlobal")
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: WaitGlobal")
  chainblocks.blockSetup(blk)
  if name != None:
    chainblocks.blockSetParam(blk, 0, CBVar(name).value)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  setPreviousBlock(blk)

def When(accept = None, isregex = None, all = None):
  blk = chainblocks.createBlock("When")
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: When")
  chainblocks.blockSetup(blk)
  if accept != None:
    chainblocks.blockSetParam(blk, 0, CBVar(accept).value)
  if isregex != None:
    chainblocks.blockSetParam(blk, 1, CBVar(isregex).value)
  if all != None:
    chainblocks.blockSetParam(blk, 2, CBVar(all).value)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  setPreviousBlock(blk)

def Repeat(forever = None, times = None, action = None):
  blk = chainblocks.createBlock("Repeat")
  if getPreviousBlock() != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Repeat")
  chainblocks.blockSetup(blk)
  if forever != None:
    chainblocks.blockSetParam(blk, 0, CBVar(forever).value)
  if times != None:
    chainblocks.blockSetParam(blk, 1, CBVar(times).value)
  if action != None:
    chainblocks.blockSetParam(blk, 2, CBVar(action).value)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  setPreviousBlock(blk)

class Math:
  def Multiply(operand = None):
    blk = chainblocks.createBlock("Math.Multiply")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Multiply")
    chainblocks.blockSetup(blk)
    if operand != None:
      chainblocks.blockSetParam(blk, 0, CBVar(operand).value)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Asin():
    blk = chainblocks.createBlock("Math.Asin")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Asin")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def RShift(operand = None):
    blk = chainblocks.createBlock("Math.RShift")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.RShift")
    chainblocks.blockSetup(blk)
    if operand != None:
      chainblocks.blockSetParam(blk, 0, CBVar(operand).value)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Abs():
    blk = chainblocks.createBlock("Math.Abs")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Abs")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Log():
    blk = chainblocks.createBlock("Math.Log")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Log")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Sqrt():
    blk = chainblocks.createBlock("Math.Sqrt")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Sqrt")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Atanh():
    blk = chainblocks.createBlock("Math.Atanh")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Atanh")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Erf():
    blk = chainblocks.createBlock("Math.Erf")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Erf")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Round():
    blk = chainblocks.createBlock("Math.Round")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Round")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Mod(operand = None):
    blk = chainblocks.createBlock("Math.Mod")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Mod")
    chainblocks.blockSetup(blk)
    if operand != None:
      chainblocks.blockSetParam(blk, 0, CBVar(operand).value)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Ceil():
    blk = chainblocks.createBlock("Math.Ceil")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Ceil")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Sin():
    blk = chainblocks.createBlock("Math.Sin")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Sin")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Xor(operand = None):
    blk = chainblocks.createBlock("Math.Xor")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Xor")
    chainblocks.blockSetup(blk)
    if operand != None:
      chainblocks.blockSetParam(blk, 0, CBVar(operand).value)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def LGamma():
    blk = chainblocks.createBlock("Math.LGamma")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.LGamma")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def And(operand = None):
    blk = chainblocks.createBlock("Math.And")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.And")
    chainblocks.blockSetup(blk)
    if operand != None:
      chainblocks.blockSetParam(blk, 0, CBVar(operand).value)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def LShift(operand = None):
    blk = chainblocks.createBlock("Math.LShift")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.LShift")
    chainblocks.blockSetup(blk)
    if operand != None:
      chainblocks.blockSetParam(blk, 0, CBVar(operand).value)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Cbrt():
    blk = chainblocks.createBlock("Math.Cbrt")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Cbrt")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Log1p():
    blk = chainblocks.createBlock("Math.Log1p")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Log1p")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Erfc():
    blk = chainblocks.createBlock("Math.Erfc")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Erfc")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Tan():
    blk = chainblocks.createBlock("Math.Tan")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Tan")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Acosh():
    blk = chainblocks.createBlock("Math.Acosh")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Acosh")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Expm1():
    blk = chainblocks.createBlock("Math.Expm1")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Expm1")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Or(operand = None):
    blk = chainblocks.createBlock("Math.Or")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Or")
    chainblocks.blockSetup(blk)
    if operand != None:
      chainblocks.blockSetParam(blk, 0, CBVar(operand).value)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Atan():
    blk = chainblocks.createBlock("Math.Atan")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Atan")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Subtract(operand = None):
    blk = chainblocks.createBlock("Math.Subtract")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Subtract")
    chainblocks.blockSetup(blk)
    if operand != None:
      chainblocks.blockSetParam(blk, 0, CBVar(operand).value)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Tanh():
    blk = chainblocks.createBlock("Math.Tanh")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Tanh")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Acos():
    blk = chainblocks.createBlock("Math.Acos")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Acos")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Asinh():
    blk = chainblocks.createBlock("Math.Asinh")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Asinh")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Add(operand = None):
    blk = chainblocks.createBlock("Math.Add")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Add")
    chainblocks.blockSetup(blk)
    if operand != None:
      chainblocks.blockSetParam(blk, 0, CBVar(operand).value)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Cosh():
    blk = chainblocks.createBlock("Math.Cosh")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Cosh")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Exp():
    blk = chainblocks.createBlock("Math.Exp")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Exp")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Divide(operand = None):
    blk = chainblocks.createBlock("Math.Divide")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Divide")
    chainblocks.blockSetup(blk)
    if operand != None:
      chainblocks.blockSetParam(blk, 0, CBVar(operand).value)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Trunc():
    blk = chainblocks.createBlock("Math.Trunc")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Trunc")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Floor():
    blk = chainblocks.createBlock("Math.Floor")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Floor")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Exp2():
    blk = chainblocks.createBlock("Math.Exp2")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Exp2")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Log2():
    blk = chainblocks.createBlock("Math.Log2")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Log2")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Sinh():
    blk = chainblocks.createBlock("Math.Sinh")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Sinh")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Cos():
    blk = chainblocks.createBlock("Math.Cos")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Cos")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Log10():
    blk = chainblocks.createBlock("Math.Log10")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.Log10")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def TGamma():
    blk = chainblocks.createBlock("Math.TGamma")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Math.TGamma")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
class Stack:
  def PopFloat2():
    blk = chainblocks.createBlock("Stack.PopFloat2")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Stack.PopFloat2")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Peek(index = None):
    blk = chainblocks.createBlock("Stack.Peek")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Stack.Peek")
    chainblocks.blockSetup(blk)
    if index != None:
      chainblocks.blockSetParam(blk, 0, CBVar(index).value)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def PopAdd():
    blk = chainblocks.createBlock("Stack.PopAdd")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Stack.PopAdd")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Push():
    blk = chainblocks.createBlock("Stack.Push")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Stack.Push")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def PopFloat4():
    blk = chainblocks.createBlock("Stack.PopFloat4")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Stack.PopFloat4")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def PopFloat3():
    blk = chainblocks.createBlock("Stack.PopFloat3")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Stack.PopFloat3")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Pop():
    blk = chainblocks.createBlock("Stack.Pop")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Stack.Pop")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def PopInt3():
    blk = chainblocks.createBlock("Stack.PopInt3")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Stack.PopInt3")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def PopDivide():
    blk = chainblocks.createBlock("Stack.PopDivide")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Stack.PopDivide")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def PopMultiply():
    blk = chainblocks.createBlock("Stack.PopMultiply")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Stack.PopMultiply")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def PopInt4():
    blk = chainblocks.createBlock("Stack.PopInt4")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Stack.PopInt4")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def PopInt2():
    blk = chainblocks.createBlock("Stack.PopInt2")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Stack.PopInt2")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def PushUnpacked():
    blk = chainblocks.createBlock("Stack.PushUnpacked")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Stack.PushUnpacked")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def PopSubtract():
    blk = chainblocks.createBlock("Stack.PopSubtract")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: Stack.PopSubtract")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
class IPC:
  def Push(name = None):
    blk = chainblocks.createBlock("IPC.Push")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: IPC.Push")
    chainblocks.blockSetup(blk)
    if name != None:
      chainblocks.blockSetParam(blk, 0, CBVar(name).value)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
  def Pop(name = None):
    blk = chainblocks.createBlock("IPC.Pop")
    if getPreviousBlock() != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(getPreviousBlock()))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(getPreviousBlock()) + ", input: IPC.Pop")
    chainblocks.blockSetup(blk)
    if name != None:
      chainblocks.blockSetParam(blk, 0, CBVar(name).value)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    setPreviousBlock(blk)
  
