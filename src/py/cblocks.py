# This file was automatically generated.

import chainblocks
from cbtypes import *

_previousBlock = None

def Py(closure = None):
  blk = chainblocks.createBlock("Py")
  global _previousBlock
  if _previousBlock != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Py")
  chainblocks.blockSetup(blk)
  if closure != None:
    chainblocks.blockSetParam(blk, 0, closure)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  _previousBlock = blk

def ReplaceText(regex = None, replacement = None):
  blk = chainblocks.createBlock("ReplaceText")
  global _previousBlock
  if _previousBlock != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: ReplaceText")
  chainblocks.blockSetup(blk)
  if regex != None:
    chainblocks.blockSetParam(blk, 0, regex)
  if replacement != None:
    chainblocks.blockSetParam(blk, 1, replacement)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  _previousBlock = blk

def ToUppercase():
  blk = chainblocks.createBlock("ToUppercase")
  global _previousBlock
  if _previousBlock != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: ToUppercase")
  chainblocks.blockSetup(blk)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  _previousBlock = blk

def RunChain(chain = None, once = None, passthrough = None):
  blk = chainblocks.createBlock("RunChain")
  global _previousBlock
  if _previousBlock != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: RunChain")
  chainblocks.blockSetup(blk)
  if chain != None:
    chainblocks.blockSetParam(blk, 0, chain)
  if once != None:
    chainblocks.blockSetParam(blk, 1, once)
  if passthrough != None:
    chainblocks.blockSetParam(blk, 2, passthrough)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  _previousBlock = blk

def ToImage(width = None, height = None, alpha = None):
  blk = chainblocks.createBlock("ToImage")
  global _previousBlock
  if _previousBlock != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: ToImage")
  chainblocks.blockSetup(blk)
  if width != None:
    chainblocks.blockSetParam(blk, 0, width)
  if height != None:
    chainblocks.blockSetParam(blk, 1, height)
  if alpha != None:
    chainblocks.blockSetParam(blk, 2, alpha)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  _previousBlock = blk

def MatchText(regex = None):
  blk = chainblocks.createBlock("MatchText")
  global _previousBlock
  if _previousBlock != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: MatchText")
  chainblocks.blockSetup(blk)
  if regex != None:
    chainblocks.blockSetParam(blk, 0, regex)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  _previousBlock = blk

def Repeat(times = None, chain = None):
  blk = chainblocks.createBlock("Repeat")
  global _previousBlock
  if _previousBlock != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Repeat")
  chainblocks.blockSetup(blk)
  if times != None:
    chainblocks.blockSetParam(blk, 0, times)
  if chain != None:
    chainblocks.blockSetParam(blk, 1, chain)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  _previousBlock = blk

def WhenNot(reject = None, isregex = None, all = None):
  blk = chainblocks.createBlock("WhenNot")
  global _previousBlock
  if _previousBlock != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: WhenNot")
  chainblocks.blockSetup(blk)
  if reject != None:
    chainblocks.blockSetParam(blk, 0, reject)
  if isregex != None:
    chainblocks.blockSetParam(blk, 1, isregex)
  if all != None:
    chainblocks.blockSetParam(blk, 2, all)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  _previousBlock = blk

def AddVariable(name = None):
  blk = chainblocks.createBlock("AddVariable")
  global _previousBlock
  if _previousBlock != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: AddVariable")
  chainblocks.blockSetup(blk)
  if name != None:
    chainblocks.blockSetParam(blk, 0, name)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  _previousBlock = blk

def ToLowercase():
  blk = chainblocks.createBlock("ToLowercase")
  global _previousBlock
  if _previousBlock != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: ToLowercase")
  chainblocks.blockSetup(blk)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  _previousBlock = blk

def SwapVariables(name1 = None, name2 = None):
  blk = chainblocks.createBlock("SwapVariables")
  global _previousBlock
  if _previousBlock != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: SwapVariables")
  chainblocks.blockSetup(blk)
  if name1 != None:
    chainblocks.blockSetParam(blk, 0, name1)
  if name2 != None:
    chainblocks.blockSetParam(blk, 1, name2)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  _previousBlock = blk

def ToFloat():
  blk = chainblocks.createBlock("ToFloat")
  global _previousBlock
  if _previousBlock != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: ToFloat")
  chainblocks.blockSetup(blk)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  _previousBlock = blk

def When(accept = None, isregex = None, all = None):
  blk = chainblocks.createBlock("When")
  global _previousBlock
  if _previousBlock != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: When")
  chainblocks.blockSetup(blk)
  if accept != None:
    chainblocks.blockSetParam(blk, 0, accept)
  if isregex != None:
    chainblocks.blockSetParam(blk, 1, isregex)
  if all != None:
    chainblocks.blockSetParam(blk, 2, all)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  _previousBlock = blk

def GetVariable(name = None):
  blk = chainblocks.createBlock("GetVariable")
  global _previousBlock
  if _previousBlock != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: GetVariable")
  chainblocks.blockSetup(blk)
  if name != None:
    chainblocks.blockSetParam(blk, 0, name)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  _previousBlock = blk

def GetItems(index = None):
  blk = chainblocks.createBlock("GetItems")
  global _previousBlock
  if _previousBlock != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: GetItems")
  chainblocks.blockSetup(blk)
  if index != None:
    chainblocks.blockSetParam(blk, 0, index)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  _previousBlock = blk

def SetVariable(name = None):
  blk = chainblocks.createBlock("SetVariable")
  global _previousBlock
  if _previousBlock != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: SetVariable")
  chainblocks.blockSetup(blk)
  if name != None:
    chainblocks.blockSetParam(blk, 0, name)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  _previousBlock = blk

def ChainRestart():
  blk = chainblocks.createBlock("ChainRestart")
  global _previousBlock
  if _previousBlock != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: ChainRestart")
  chainblocks.blockSetup(blk)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  _previousBlock = blk

def WaitGlobal(name = None):
  blk = chainblocks.createBlock("WaitGlobal")
  global _previousBlock
  if _previousBlock != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: WaitGlobal")
  chainblocks.blockSetup(blk)
  if name != None:
    chainblocks.blockSetParam(blk, 0, name)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  _previousBlock = blk

def If(operator = None, operand = None, true = None, false = None, passthrough = None):
  blk = chainblocks.createBlock("If")
  global _previousBlock
  if _previousBlock != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: If")
  chainblocks.blockSetup(blk)
  if operator != None:
    chainblocks.blockSetParam(blk, 0, operator)
  if operand != None:
    chainblocks.blockSetParam(blk, 1, operand)
  if true != None:
    chainblocks.blockSetParam(blk, 2, true)
  if false != None:
    chainblocks.blockSetParam(blk, 3, false)
  if passthrough != None:
    chainblocks.blockSetParam(blk, 4, passthrough)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  _previousBlock = blk

def Const(value = None):
  blk = chainblocks.createBlock("Const")
  global _previousBlock
  if _previousBlock != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Const")
  chainblocks.blockSetup(blk)
  if value != None:
    chainblocks.blockSetParam(blk, 0, value)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  _previousBlock = blk

def WaitChain(chain = None, once = None, passthrough = None):
  blk = chainblocks.createBlock("WaitChain")
  global _previousBlock
  if _previousBlock != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: WaitChain")
  chainblocks.blockSetup(blk)
  if chain != None:
    chainblocks.blockSetParam(blk, 0, chain)
  if once != None:
    chainblocks.blockSetParam(blk, 1, once)
  if passthrough != None:
    chainblocks.blockSetParam(blk, 2, passthrough)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  _previousBlock = blk

def ChainStop():
  blk = chainblocks.createBlock("ChainStop")
  global _previousBlock
  if _previousBlock != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: ChainStop")
  chainblocks.blockSetup(blk)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  _previousBlock = blk

def ToString():
  blk = chainblocks.createBlock("ToString")
  global _previousBlock
  if _previousBlock != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: ToString")
  chainblocks.blockSetup(blk)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  _previousBlock = blk

def Log():
  blk = chainblocks.createBlock("Log")
  global _previousBlock
  if _previousBlock != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Log")
  chainblocks.blockSetup(blk)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  _previousBlock = blk

def Msg(message = None):
  blk = chainblocks.createBlock("Msg")
  global _previousBlock
  if _previousBlock != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Msg")
  chainblocks.blockSetup(blk)
  if message != None:
    chainblocks.blockSetParam(blk, 0, message)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  _previousBlock = blk

def SetGlobal(name = None):
  blk = chainblocks.createBlock("SetGlobal")
  global _previousBlock
  if _previousBlock != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: SetGlobal")
  chainblocks.blockSetup(blk)
  if name != None:
    chainblocks.blockSetParam(blk, 0, name)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  _previousBlock = blk

def Sleep(time = None):
  blk = chainblocks.createBlock("Sleep")
  global _previousBlock
  if _previousBlock != None:
    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
    if not validateConnection(prevOutInfo, currInInfo):
      raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Sleep")
  chainblocks.blockSetup(blk)
  if time != None:
    chainblocks.blockSetParam(blk, 0, time)
  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
  _previousBlock = blk

class Math:
  def Round():
    blk = chainblocks.createBlock("Math.Round")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Round")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Trunc():
    blk = chainblocks.createBlock("Math.Trunc")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Trunc")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Floor():
    blk = chainblocks.createBlock("Math.Floor")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Floor")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Exp2():
    blk = chainblocks.createBlock("Math.Exp2")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Exp2")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Subtract(operand = None):
    blk = chainblocks.createBlock("Math.Subtract")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Subtract")
    chainblocks.blockSetup(blk)
    if operand != None:
      chainblocks.blockSetParam(blk, 0, operand)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def RShift(operand = None):
    blk = chainblocks.createBlock("Math.RShift")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.RShift")
    chainblocks.blockSetup(blk)
    if operand != None:
      chainblocks.blockSetParam(blk, 0, operand)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Erfc():
    blk = chainblocks.createBlock("Math.Erfc")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Erfc")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Expm1():
    blk = chainblocks.createBlock("Math.Expm1")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Expm1")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Multiply(operand = None):
    blk = chainblocks.createBlock("Math.Multiply")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Multiply")
    chainblocks.blockSetup(blk)
    if operand != None:
      chainblocks.blockSetParam(blk, 0, operand)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Log10():
    blk = chainblocks.createBlock("Math.Log10")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Log10")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def And(operand = None):
    blk = chainblocks.createBlock("Math.And")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.And")
    chainblocks.blockSetup(blk)
    if operand != None:
      chainblocks.blockSetParam(blk, 0, operand)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Or(operand = None):
    blk = chainblocks.createBlock("Math.Or")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Or")
    chainblocks.blockSetup(blk)
    if operand != None:
      chainblocks.blockSetParam(blk, 0, operand)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Abs():
    blk = chainblocks.createBlock("Math.Abs")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Abs")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Asin():
    blk = chainblocks.createBlock("Math.Asin")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Asin")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def LShift(operand = None):
    blk = chainblocks.createBlock("Math.LShift")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.LShift")
    chainblocks.blockSetup(blk)
    if operand != None:
      chainblocks.blockSetParam(blk, 0, operand)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Tanh():
    blk = chainblocks.createBlock("Math.Tanh")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Tanh")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Tan():
    blk = chainblocks.createBlock("Math.Tan")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Tan")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Erf():
    blk = chainblocks.createBlock("Math.Erf")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Erf")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Atan():
    blk = chainblocks.createBlock("Math.Atan")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Atan")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Log():
    blk = chainblocks.createBlock("Math.Log")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Log")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Ceil():
    blk = chainblocks.createBlock("Math.Ceil")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Ceil")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Add(operand = None):
    blk = chainblocks.createBlock("Math.Add")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Add")
    chainblocks.blockSetup(blk)
    if operand != None:
      chainblocks.blockSetParam(blk, 0, operand)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Divide(operand = None):
    blk = chainblocks.createBlock("Math.Divide")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Divide")
    chainblocks.blockSetup(blk)
    if operand != None:
      chainblocks.blockSetParam(blk, 0, operand)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Xor(operand = None):
    blk = chainblocks.createBlock("Math.Xor")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Xor")
    chainblocks.blockSetup(blk)
    if operand != None:
      chainblocks.blockSetParam(blk, 0, operand)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Asinh():
    blk = chainblocks.createBlock("Math.Asinh")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Asinh")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Mod(operand = None):
    blk = chainblocks.createBlock("Math.Mod")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Mod")
    chainblocks.blockSetup(blk)
    if operand != None:
      chainblocks.blockSetParam(blk, 0, operand)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Exp():
    blk = chainblocks.createBlock("Math.Exp")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Exp")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Log2():
    blk = chainblocks.createBlock("Math.Log2")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Log2")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Sin():
    blk = chainblocks.createBlock("Math.Sin")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Sin")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Log1p():
    blk = chainblocks.createBlock("Math.Log1p")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Log1p")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Sqrt():
    blk = chainblocks.createBlock("Math.Sqrt")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Sqrt")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Cbrt():
    blk = chainblocks.createBlock("Math.Cbrt")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Cbrt")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Cos():
    blk = chainblocks.createBlock("Math.Cos")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Cos")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def LGamma():
    blk = chainblocks.createBlock("Math.LGamma")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.LGamma")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Sinh():
    blk = chainblocks.createBlock("Math.Sinh")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Sinh")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Acos():
    blk = chainblocks.createBlock("Math.Acos")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Acos")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Cosh():
    blk = chainblocks.createBlock("Math.Cosh")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Cosh")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Acosh():
    blk = chainblocks.createBlock("Math.Acosh")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Acosh")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Atanh():
    blk = chainblocks.createBlock("Math.Atanh")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.Atanh")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def TGamma():
    blk = chainblocks.createBlock("Math.TGamma")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Math.TGamma")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
class Stack:
  def PopFloat3():
    blk = chainblocks.createBlock("Stack.PopFloat3")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Stack.PopFloat3")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def PopInt3():
    blk = chainblocks.createBlock("Stack.PopInt3")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Stack.PopInt3")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def PopMultiply():
    blk = chainblocks.createBlock("Stack.PopMultiply")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Stack.PopMultiply")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def PopInt2():
    blk = chainblocks.createBlock("Stack.PopInt2")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Stack.PopInt2")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Push():
    blk = chainblocks.createBlock("Stack.Push")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Stack.Push")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Pop():
    blk = chainblocks.createBlock("Stack.Pop")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Stack.Pop")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def PushUnpacked():
    blk = chainblocks.createBlock("Stack.PushUnpacked")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Stack.PushUnpacked")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def PopDivide():
    blk = chainblocks.createBlock("Stack.PopDivide")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Stack.PopDivide")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def PopFloat4():
    blk = chainblocks.createBlock("Stack.PopFloat4")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Stack.PopFloat4")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def PopAdd():
    blk = chainblocks.createBlock("Stack.PopAdd")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Stack.PopAdd")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def PopFloat2():
    blk = chainblocks.createBlock("Stack.PopFloat2")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Stack.PopFloat2")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def PopSubtract():
    blk = chainblocks.createBlock("Stack.PopSubtract")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Stack.PopSubtract")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def PopInt4():
    blk = chainblocks.createBlock("Stack.PopInt4")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Stack.PopInt4")
    chainblocks.blockSetup(blk)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Peek(index = None):
    blk = chainblocks.createBlock("Stack.Peek")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: Stack.Peek")
    chainblocks.blockSetup(blk)
    if index != None:
      chainblocks.blockSetParam(blk, 0, index)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
class IPC:
  def Push(name = None):
    blk = chainblocks.createBlock("IPC.Push")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: IPC.Push")
    chainblocks.blockSetup(blk)
    if name != None:
      chainblocks.blockSetParam(blk, 0, name)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
  def Pop(name = None):
    blk = chainblocks.createBlock("IPC.Pop")
    global _previousBlock
    if _previousBlock != None:
      prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))
      currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))
      if not validateConnection(prevOutInfo, currInInfo):
        raise Exception("Blocks do not connect, output: " + chainblocks.blockName(_previousBlock) + ", input: IPC.Pop")
    chainblocks.blockSetup(blk)
    if name != None:
      chainblocks.blockSetParam(blk, 0, name)
    chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)
    _previousBlock = blk
  
