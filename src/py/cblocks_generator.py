from chainblocks.cbtypes import CBType
from chainblocks.chainblocks import *

cbNamespaces = dict()
blacklist = ["NimClosure"]

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
      if paramsString == "":
        paramsString = "{} = None".format(pname)
      else:
        paramsString += ", {} = None".format(pname)
  
  result =  "{}@staticmethod\n".format(indent)
  result =  "{}def {}({}):\n".format(indent, name, paramsString)

  # Create and setup
  result += "{}  blk = chainblocks.createBlock(\"{}\")\n".format(indent, fullName)
  
  # Check first if we can connect with the previous block
  result += "{}  global _previousBlock\n".format(indent)
  result += "{}  if _previousBlock != None:\n".format(indent)
  result += "{}    prevOutInfo = chainblocks.unpackTypesInfo(chainblocks.blockOutputTypes(_previousBlock))\n".format(indent)
  result += "{}    currInInfo = chainblocks.unpackTypesInfo(chainblocks.blockInputTypes(blk))\n".format(indent)
  result += "{}    if not validateConnection(prevOutInfo, currInInfo):\n".format(indent)
  result += "{}      raise Exception(\"Blocks do not connect, output: \" + chainblocks.blockName(_previousBlock) + \", input: {}\")\n".format(indent, fullName)
  
  # Setup
  result += "{}  chainblocks.blockSetup(blk)\n".format(indent)
  
  # Evaluate params
  if parametersp != None:
    pindex = 0
    for param in parameters:
      pname = param[0].lower()
      result += "{}  if {} != None:\n".format(indent, pname)
      result += "{}    chainblocks.blockSetParam(blk, {}, CBVar({}).value)\n".format(indent, str(pindex), pname)     
      pindex = pindex + 1
  
  # After setting initial params add to the chain
  result += "{}  chainblocks.chainAddBlock(chainblocks.getCurrentChain(), blk)\n".format(indent)
  result += "{}  _previousBlock = blk\n".format(indent)
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
f.write("from .cbtypes import *\n\n")
f.write("_previousBlock = None\n\n")
for name, code in cbNamespaces.items():
  if name != "":
    f.write("class {}:\n".format(name))
    f.write(code)
  else:
    f.write(code)