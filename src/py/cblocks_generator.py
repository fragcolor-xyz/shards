from chainblocks.cbcore import CBType
from chainblocks.chainblocks import *

cbNamespaces = dict()
blacklist = ["NimClosure"]
reservedWords = ["global", "class"]

def processBlock(name, hasNamespace, fullName):
  indent = "  " if hasNamespace else ""
  # Create a reflection block
  blk = createBlock(fullName)
  # Grab all info
  parametersp = blockParameters(blk)
  parameters = None
  if parametersp != None:
    parameters = unpackParametersInfo(parametersp)
  # we should also check exposed, consumed... but those can easy change at runtime
  # Destroy since we don't need it anymore
  blockDestroy(blk)

  paramsString = ""
  if parametersp != None:
    for param in parameters:
      pname = param[0].lower()
      if pname in reservedWords:
        pname = "_" + pname
      if paramsString == "":
        paramsString = "{} = None".format(pname)
      else:
        paramsString += ", {} = None".format(pname)
  
  result =  "{}@staticmethod\n".format(indent)
  result =  "{}def {}({}):\n".format(indent, name, paramsString)

  # Create
  result += "{}  blk = CBRuntimeBlock(chainblocks.createBlock(\"{}\"))\n".format(indent, fullName)
  # Setup
  result += "{}  chainblocks.blockSetup(blk.block)\n".format(indent)
  
  # Evaluate params
  if parametersp != None:
    pindex = 0
    for param in parameters:
      pname = param[0].lower()
      if pname in reservedWords:
        pname = "_" + pname
      result += "{}  if {} != None:\n".format(indent, pname)
      result += "{}    chainblocks.blockSetParam(blk.block, {}, CBVar({}).value)\n".format(indent, str(pindex), pname)     
      pindex = pindex + 1
  
  # After setting initial params add to the chain
  result += "{}  return blk\n".format(indent)
  result += "{}\n".format(indent)
  return result

for name in blocks():
  if name in blacklist:
    continue
  
  print("Processing: " + name)
  namespace = name.split(".")
  if len(namespace) == 1:
    if "" not in cbNamespaces:
      cbNamespaces[""] = ""
    
    cbNamespaces[""] += processBlock(name, False, name)
  
  elif len(namespace) == 2:
    if namespace[0] not in cbNamespaces:
      cbNamespaces[namespace[0]] = ""
      print("added namespace: " + namespace[0])
    
    cbNamespaces[namespace[0]] += processBlock(namespace[1], True, name)

f = open("cblocks.py", "w")
f.write("# This file was automatically generated.\n\n")
f.write("import chainblocks\n")
f.write("from .cbcore import *\n\n")
for name, code in cbNamespaces.items():
  if name != "":
    f.write("class {}:\n".format(name))
    f.write(code)
  else:
    f.write(code)