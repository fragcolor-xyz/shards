function run() {
  var v1 = new Var(1.0, 2.0, 3.0) // Float3
  log(v1)
  var v2 = new Var(2.0, 3.0) // Float2
  log(v2)
  var v3 = new Var(1, 2, 3, 10) // Int4
  log(v3)
  var v4 = new Var("Hello chainblocks!") // String
  log(v4)
  
  var blk = new Block("Const")
  blk.setParam(0, v4)
  log(v4)
  log(v4)
  
  const blockName = blk.name
  log(blockName)
  log(blk.getParam(0))
  
  var upper = new Block("ToUppercase")
  var logblk = new Block("Log")
  var sqrt = new Block("Math.Sqrt")
  
  var chain = new Chain("mainChain")
  chain.looped = true
  var node = new Node()
  
  chain.addBlock(blk)
  // chain.addBlock(sqrt)
  // chain.addBlock(upper)
  chain.addBlock(logblk)
  
  try
  {
    node.schedule(chain)
  }
  catch(e)
  {
    log(e)
  }
  node.tick()
  node.tick()
  node.tick()
  node.tick()
}

try
{
  run()
}
catch(e)
{
  log(e)
}