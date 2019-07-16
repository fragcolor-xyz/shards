import chainblocks
from inspect import getsource
from functools import wraps
from ast import parse, NodeTransformer, Attribute, Name
from io import StringIO
from tokenize import generate_tokens, untokenize, INDENT

def _dump(obj):
  for attr in dir(obj):
    print("obj.%s = %r" % (attr, getattr(obj, attr)))

def _dedent(s):
  result = [t[:2] for t in generate_tokens(StringIO(s).readline)]
  # set initial indent to 0 if any
  if result[0][0] == INDENT:
      result[0] = (INDENT, '')
  return untokenize(result)

class Transformer(NodeTransformer):
  def __init__(self, chain):
    self.src = ""
    self.indent = 0
    self.chain = chain

  def build(self, node):
    self.visit(node)

  def visit_Call(self, call):
    blockName = ""
    if type(call.func) is Attribute: # we got a block namespace!
      blockName = call.func.value.id + "." + call.func.attr
    elif type(call.func) is Name:
      blockName = call.func.id

    blk = chainblocks.createBlock(blockName)
    print("Created block: " + chainblocks.blockName(blk))
    chainblocks.addBlock(self.chain, blk)

def chain(func):
  def createChain():
    previousChain = chainblocks.getCurrentChain()
    
    chain = chainblocks.createChain(func.__name__)
    chainblocks.setCurrentChain(chain)
    
    _source = getsource(func)
    # Skip first line (has the decorator) and dedent
    _source = _dedent(_source.split("\n",1)[1])
    _ast = parse(_source)
    transform = Transformer(chain)
    _source = transform.build(_ast)
    
    if previousChain != None:
      chainblocks.setCurrentChain(previousChain)
    return chain
  return createChain

def start(chain, looped = False, unsafe = False):
  chainblocks.start(chain, looped, unsafe)

if __name__ == '__main__':
  @chain
  def MainChain():
    Const()
    Stack.Push()
    Log()
  
  main = MainChain()
  print("Starting chain!")
  start(main)
  print("Stopping chain!")

  blk = chainblocks.createBlock("Const")
  print("Created block: " + chainblocks.blockName(blk))
  chainblocks.blockSetParam(blk, 0, (13, "Hello world"))
  print(chainblocks.blockGetParam(blk, 0))
  chainblocks.blockSetParam(blk, 0, (16, [(13, "Hello world 1"), (13, "Hello world 2")]))
  print(chainblocks.blockGetParam(blk, 0))

  # TODO, from this generate all stub classes with proper documented arguments
  # Also this allows runtime checks for input/output/params!
  # print("Blocks:")
  # for blk in chainblocks.blocks():
  #   print(blk)
  
  logblock = chainblocks.createBlock("Log")
  
