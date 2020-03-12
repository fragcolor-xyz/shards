import sys

## This could also work!!

# def inputTypes():
#   print("inputTypes")
#   sys.stdout.flush()
#   return ["Int"]

# def outputTypes():
#   print("outputTypes")
#   sys.stdout.flush()
#   return ["Int"]

# def activate(inputs):
#   to_add = 10
#   return inputs + to_add

## Or better with a class obj context!

## OPTIONAL STUFF

class MyBlock1:
  def __init__(self):
    print("MyBlock1 created!")
    sys.stdout.flush()
    self.p1 = ""

def setup():
  print("setup")
  sys.stdout.flush()
  return MyBlock1()

def inputTypes(self):
  print("inputTypes")
  sys.stdout.flush()
  return ["Int"]

def outputTypes(self):
  print("outputTypes")
  sys.stdout.flush()
  return ["Int"]

def activate(self, inputs):
  print(self)
  sys.stdout.flush()
  to_add = 10
  self.pause(1.0)
  return inputs + to_add

## optional stuff

def compose(self, inputType):
  print("compose")
  sys.stdout.flush()
  return "Int"

def parameters(self):
  print("parameters")
  sys.stdout.flush()
  return [("Param1", "Text to print", ["String", "Int"])]

def setParam(self, index, value):
  if index == 0:
    self.p1 = value
    print(self.p1)
    sys.stdout.flush()

def getParam(self, index):
  if index == 0:
    return self.p1
