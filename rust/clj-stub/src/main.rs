extern crate chainblocks;
use chainblocks::core::init;
use chainblocks::core::getBlocks;

fn main() {
  init();
  let blocks = getBlocks();
  for name in blocks {
    println!("Got block: {:?}", name);
  }
}
