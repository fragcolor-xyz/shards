extern crate chainblocks;
use chainblocks::core::createBlock;
use chainblocks::core::init;
use chainblocks::core::getBlocks;

fn main() {
  init();
  let blocks = getBlocks();
  for name in blocks {
    let blk = createBlock(name.to_str().unwrap());
    let params = blk.parameters();
    println!("Got block: {:?}", name);
  }
}
