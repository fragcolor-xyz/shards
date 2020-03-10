import sys

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
  return inputs + 10
