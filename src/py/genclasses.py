from cbtypes import CBType
from chainblocks import *

cbNamespaces = dict()
blacklist = ["NimClosure"]

def processBlock(name, hasNamespace, fullName):
  indent = "  " if hasNamespace else ""
  # Create a reflection block
  blk = createBlock(fullName)
  # Grab all info
  inputTypes = unpackTypesInfo(blockInputTypes(blk))
  outputTypes = unpackTypesInfo(blockOutputTypes(blk))
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
      if paramsString == "":
        paramsString = "{} = None".format(pname)
      else:
        paramsString += ", {} = None".format(pname)
  
  result =  "{}def {}({}):\n".format(indent, name, paramsString)
  
  # Create and setup
  result += "{}  blk = chainblocks.createBlock(\"{}\")\n".format(indent, fullName)
  result += "{}  chainblocks.blockSetup(blk)\n".format(indent, fullName)
  
  # Evaluate params
  if parametersp != None:
    pindex = 0
    for param in parameters:
      pname = param[0].lower()
      result += "{}  if {} != None:\n".format(indent, pname)
      result += "{}    chainblocks.blockSetParam(blk, {}, {})\n".format(indent, str(pindex), pname)     
      pindex = pindex + 1
  
  # After setting initial params add to the chain
  result += "{}  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)\n".format(indent)
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
f.write("import chainblocks\n\n")
for name, code in cbNamespaces.items():
  if name != "":
    f.write("class {}:\n".format(name))
    f.write(code)
  else:
    f.write(code)