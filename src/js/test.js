import { chain, Msg, Log, Repeat, SetTableValue, GetTableValue } from "./blocks";

try
{
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
    
    var c = new Chain("mainChain")
    c.looped = true
    var node = new Node()
    
    c.addBlock(blk)
    // c.addBlock(sqrt)
    c.addBlock(upper)
    c.addBlock(logblk)
    
    try
    {
      node.schedule(c)
    }
    catch(e)
    {
      log(e)
    }
    node.tick()
    node.tick()
    node.tick()
    node.tick()

    node.schedule(chain([
      Msg("chain worked!"),
      Msg("Yep yep"),
      Repeat(
        [
          Msg("Repeating... this..."),
          77,
          Log(),
          Repeat(
            [
              Msg("Sub repeating!"),
              99,
              Log()
            ],
            5
          )
        ],
        5
      ),
      "JS is not too bad uh!",
      Log(),
      "Tables are nice",
      SetTableValue(
        "table1",
        "myKey"
      ),
      "This value will not show up!",
      GetTableValue(
        "table1",
        "myKey"
      ),
      Log()
    ]))
  }

  run()
}
catch(e)
{
  log(e)
}