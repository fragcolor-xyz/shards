import sys

## This could also work!!

# def inputTypes():
#   print("inputTypes")
#   sys.stdout.flush()
#   return ("Int")

# def outputTypes():
#   print("outputTypes")
#   sys.stdout.flush()
#   return ("Int")

# def activate(inputs):
#   to_add = 10
#   return inputs + to_add

## Or better with a class obj context!

## OPTIONAL STUFF

class MyBlock1:
  pass

def setup():
  print("setup")
  sys.stdout.flush()
  return MyBlock1()

def inputTypes(self):
  print("inputTypes")
  sys.stdout.flush()
  return ("Int")

def outputTypes(self):
  print("outputTypes")
  sys.stdout.flush()
  return ("Int")

def activate(self, inputs):
  print(self)
  sys.stdout.flush()
  to_add = 10
  self.pause(1.0)
  return inputs + to_add

## optional parameters

def parameters(self):
  print("parameters")
  sys.stdout.flush()
  return (("Param1", "Help text", ("Int")))

def setParam(self, index, value):
  pass

def getParam(self, index):
  return None
