use crate::core::registerShard;
use crate::types::ExposedTypes;
use crate::types::ParamVar;
use crate::types::ShardsVar;

mod menu;

struct MenuBar {
  parents: ParamVar,
  requiring: ExposedTypes,
  contents: ShardsVar,
}

pub fn registerShards() {
  registerShard::<MenuBar>();
}
