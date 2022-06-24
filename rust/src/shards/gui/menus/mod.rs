use crate::core::registerShard;
use crate::types::ExposedTypes;
use crate::types::ParamVar;
use crate::types::ShardsVar;

mod menu;

struct Menu {
  parents: ParamVar,
  requiring: ExposedTypes,
  title: ParamVar,
  contents: ShardsVar,
}

struct MenuBar {
  parents: ParamVar,
  requiring: ExposedTypes,
  contents: ShardsVar,
}

pub fn registerShards() {
  registerShard::<Menu>();
  registerShard::<MenuBar>();
}
