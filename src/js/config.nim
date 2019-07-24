import macros

const extraBlocks {.strdefine.} = ""

macro importExtras*(): untyped =
  var
    moduleNode = newIdentNode(extraBlocks)
    moduleName = $moduleNode
  if moduleName != "":
    result = quote do:
      import `moduleNode`
  else:
    result = quote do:
      import ../nim/chainblocks

importExtras()
