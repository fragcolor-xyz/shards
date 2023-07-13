use shards::core::registerShard;
use shards::types::ExposedTypes;
use shards::types::ParamVar;
use shards::types::ShardsVar;

mod menu;

struct CloseMenu {
  parents: ParamVar,
  requiring: ExposedTypes,
}

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
  registerShard::<CloseMenu>();
  registerShard::<Menu>();
  registerShard::<MenuBar>();
}
