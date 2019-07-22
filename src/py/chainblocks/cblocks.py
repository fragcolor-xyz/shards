# This file was automatically generated.

import chainblocks
from .cbcore import *

def GetItems(index = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("GetItems"))
  chainblocks.blockSetup(blk.block)
  if index != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(index).value)
  return blk

def ToImage(width = None, height = None, channels = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("ToImage"))
  chainblocks.blockSetup(blk.block)
  if width != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(width).value)
  if height != None:
    chainblocks.blockSetParam(blk.block, 1, CBVar(height).value)
  if channels != None:
    chainblocks.blockSetParam(blk.block, 2, CBVar(channels).value)
  return blk

def Const(value = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("Const"))
  chainblocks.blockSetup(blk.block)
  if value != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(value).value)
  return blk

def SetGlobal(name = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("SetGlobal"))
  chainblocks.blockSetup(blk.block)
  if name != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(name).value)
  return blk

def SwapVariables(name1 = None, name2 = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("SwapVariables"))
  chainblocks.blockSetup(blk.block)
  if name1 != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(name1).value)
  if name2 != None:
    chainblocks.blockSetParam(blk.block, 1, CBVar(name2).value)
  return blk

def WhenNot(reject = None, isregex = None, all = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("WhenNot"))
  chainblocks.blockSetup(blk.block)
  if reject != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(reject).value)
  if isregex != None:
    chainblocks.blockSetParam(blk.block, 1, CBVar(isregex).value)
  if all != None:
    chainblocks.blockSetParam(blk.block, 2, CBVar(all).value)
  return blk

def If(operator = None, operand = None, true = None, false = None, passthrough = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("If"))
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
  return blk

def ToString():
  blk = CBRuntimeBlock(chainblocks.createBlock("ToString"))
  chainblocks.blockSetup(blk.block)
  return blk

def ToUppercase():
  blk = CBRuntimeBlock(chainblocks.createBlock("ToUppercase"))
  chainblocks.blockSetup(blk.block)
  return blk

def RunChain(chain = None, once = None, passthrough = None, detached = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("RunChain"))
  chainblocks.blockSetup(blk.block)
  if chain != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(chain).value)
  if once != None:
    chainblocks.blockSetParam(blk.block, 1, CBVar(once).value)
  if passthrough != None:
    chainblocks.blockSetParam(blk.block, 2, CBVar(passthrough).value)
  if detached != None:
    chainblocks.blockSetParam(blk.block, 3, CBVar(detached).value)
  return blk

def ChainStop():
  blk = CBRuntimeBlock(chainblocks.createBlock("ChainStop"))
  chainblocks.blockSetup(blk.block)
  return blk

def ToLowercase():
  blk = CBRuntimeBlock(chainblocks.createBlock("ToLowercase"))
  chainblocks.blockSetup(blk.block)
  return blk

def GetVariable(name = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("GetVariable"))
  chainblocks.blockSetup(blk.block)
  if name != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(name).value)
  return blk

def Py(closure = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("Py"))
  chainblocks.blockSetup(blk.block)
  if closure != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(closure).value)
  return blk

def AddVariable(name = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("AddVariable"))
  chainblocks.blockSetup(blk.block)
  if name != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(name).value)
  return blk

def Msg(message = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("Msg"))
  chainblocks.blockSetup(blk.block)
  if message != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(message).value)
  return blk

def WaitChain(chain = None, once = None, passthrough = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("WaitChain"))
  chainblocks.blockSetup(blk.block)
  if chain != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(chain).value)
  if once != None:
    chainblocks.blockSetParam(blk.block, 1, CBVar(once).value)
  if passthrough != None:
    chainblocks.blockSetParam(blk.block, 2, CBVar(passthrough).value)
  return blk

def ChainRestart():
  blk = CBRuntimeBlock(chainblocks.createBlock("ChainRestart"))
  chainblocks.blockSetup(blk.block)
  return blk

def MatchText(regex = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("MatchText"))
  chainblocks.blockSetup(blk.block)
  if regex != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(regex).value)
  return blk

def Sleep(time = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("Sleep"))
  chainblocks.blockSetup(blk.block)
  if time != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(time).value)
  return blk

def ToFloat():
  blk = CBRuntimeBlock(chainblocks.createBlock("ToFloat"))
  chainblocks.blockSetup(blk.block)
  return blk

def ReplaceText(regex = None, replacement = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("ReplaceText"))
  chainblocks.blockSetup(blk.block)
  if regex != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(regex).value)
  if replacement != None:
    chainblocks.blockSetParam(blk.block, 1, CBVar(replacement).value)
  return blk

def SetVariable(name = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("SetVariable"))
  chainblocks.blockSetup(blk.block)
  if name != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(name).value)
  return blk

def Log():
  blk = CBRuntimeBlock(chainblocks.createBlock("Log"))
  chainblocks.blockSetup(blk.block)
  return blk

def WaitGlobal(name = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("WaitGlobal"))
  chainblocks.blockSetup(blk.block)
  if name != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(name).value)
  return blk

def When(accept = None, isregex = None, all = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("When"))
  chainblocks.blockSetup(blk.block)
  if accept != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(accept).value)
  if isregex != None:
    chainblocks.blockSetParam(blk.block, 1, CBVar(isregex).value)
  if all != None:
    chainblocks.blockSetParam(blk.block, 2, CBVar(all).value)
  return blk

def Repeat(forever = None, times = None, action = None):
  blk = CBRuntimeBlock(chainblocks.createBlock("Repeat"))
  chainblocks.blockSetup(blk.block)
  if forever != None:
    chainblocks.blockSetParam(blk.block, 0, CBVar(forever).value)
  if times != None:
    chainblocks.blockSetParam(blk.block, 1, CBVar(times).value)
  if action != None:
    chainblocks.blockSetParam(blk.block, 2, CBVar(action).value)
  return blk

class Math:
  def Multiply(operand = None):
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Multiply"))
    chainblocks.blockSetup(blk.block)
    if operand != None:
      chainblocks.blockSetParam(blk.block, 0, CBVar(operand).value)
    return blk
  
  def Asin():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Asin"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def RShift(operand = None):
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.RShift"))
    chainblocks.blockSetup(blk.block)
    if operand != None:
      chainblocks.blockSetParam(blk.block, 0, CBVar(operand).value)
    return blk
  
  def Abs():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Abs"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def Log():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Log"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def Sqrt():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Sqrt"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def Atanh():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Atanh"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def Erf():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Erf"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def Round():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Round"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def Mod(operand = None):
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Mod"))
    chainblocks.blockSetup(blk.block)
    if operand != None:
      chainblocks.blockSetParam(blk.block, 0, CBVar(operand).value)
    return blk
  
  def Ceil():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Ceil"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def Sin():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Sin"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def Xor(operand = None):
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Xor"))
    chainblocks.blockSetup(blk.block)
    if operand != None:
      chainblocks.blockSetParam(blk.block, 0, CBVar(operand).value)
    return blk
  
  def LGamma():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.LGamma"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def And(operand = None):
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.And"))
    chainblocks.blockSetup(blk.block)
    if operand != None:
      chainblocks.blockSetParam(blk.block, 0, CBVar(operand).value)
    return blk
  
  def LShift(operand = None):
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.LShift"))
    chainblocks.blockSetup(blk.block)
    if operand != None:
      chainblocks.blockSetParam(blk.block, 0, CBVar(operand).value)
    return blk
  
  def Cbrt():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Cbrt"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def Log1p():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Log1p"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def Erfc():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Erfc"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def Tan():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Tan"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def Acosh():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Acosh"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def Expm1():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Expm1"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def Or(operand = None):
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Or"))
    chainblocks.blockSetup(blk.block)
    if operand != None:
      chainblocks.blockSetParam(blk.block, 0, CBVar(operand).value)
    return blk
  
  def Atan():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Atan"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def Subtract(operand = None):
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Subtract"))
    chainblocks.blockSetup(blk.block)
    if operand != None:
      chainblocks.blockSetParam(blk.block, 0, CBVar(operand).value)
    return blk
  
  def Tanh():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Tanh"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def Acos():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Acos"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def Asinh():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Asinh"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def Add(operand = None):
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Add"))
    chainblocks.blockSetup(blk.block)
    if operand != None:
      chainblocks.blockSetParam(blk.block, 0, CBVar(operand).value)
    return blk
  
  def Cosh():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Cosh"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def Exp():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Exp"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def Divide(operand = None):
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Divide"))
    chainblocks.blockSetup(blk.block)
    if operand != None:
      chainblocks.blockSetParam(blk.block, 0, CBVar(operand).value)
    return blk
  
  def Trunc():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Trunc"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def Floor():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Floor"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def Exp2():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Exp2"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def Log2():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Log2"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def Sinh():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Sinh"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def Cos():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Cos"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def Log10():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.Log10"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def TGamma():
    blk = CBRuntimeBlock(chainblocks.createBlock("Math.TGamma"))
    chainblocks.blockSetup(blk.block)
    return blk
  
class Stack:
  def PopFloat2():
    blk = CBRuntimeBlock(chainblocks.createBlock("Stack.PopFloat2"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def Peek(index = None):
    blk = CBRuntimeBlock(chainblocks.createBlock("Stack.Peek"))
    chainblocks.blockSetup(blk.block)
    if index != None:
      chainblocks.blockSetParam(blk.block, 0, CBVar(index).value)
    return blk
  
  def PopAdd():
    blk = CBRuntimeBlock(chainblocks.createBlock("Stack.PopAdd"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def Push():
    blk = CBRuntimeBlock(chainblocks.createBlock("Stack.Push"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def PopFloat4():
    blk = CBRuntimeBlock(chainblocks.createBlock("Stack.PopFloat4"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def PopFloat3():
    blk = CBRuntimeBlock(chainblocks.createBlock("Stack.PopFloat3"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def Pop():
    blk = CBRuntimeBlock(chainblocks.createBlock("Stack.Pop"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def PopInt3():
    blk = CBRuntimeBlock(chainblocks.createBlock("Stack.PopInt3"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def PopDivide():
    blk = CBRuntimeBlock(chainblocks.createBlock("Stack.PopDivide"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def PopMultiply():
    blk = CBRuntimeBlock(chainblocks.createBlock("Stack.PopMultiply"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def PopInt4():
    blk = CBRuntimeBlock(chainblocks.createBlock("Stack.PopInt4"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def PopInt2():
    blk = CBRuntimeBlock(chainblocks.createBlock("Stack.PopInt2"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def PushUnpacked():
    blk = CBRuntimeBlock(chainblocks.createBlock("Stack.PushUnpacked"))
    chainblocks.blockSetup(blk.block)
    return blk
  
  def PopSubtract():
    blk = CBRuntimeBlock(chainblocks.createBlock("Stack.PopSubtract"))
    chainblocks.blockSetup(blk.block)
    return blk
  
class IPC:
  def Push(name = None):
    blk = CBRuntimeBlock(chainblocks.createBlock("IPC.Push"))
    chainblocks.blockSetup(blk.block)
    if name != None:
      chainblocks.blockSetParam(blk.block, 0, CBVar(name).value)
    return blk
  
  def Pop(name = None):
    blk = CBRuntimeBlock(chainblocks.createBlock("IPC.Pop"))
    chainblocks.blockSetup(blk.block)
    if name != None:
      chainblocks.blockSetParam(blk.block, 0, CBVar(name).value)
    return blk
  
