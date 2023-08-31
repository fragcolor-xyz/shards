use shards::core::register_legacy_shard;
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

pub fn register_shards() {
  register_legacy_shard::<CloseMenu>();
  register_legacy_shard::<Menu>();
  register_legacy_shard::<MenuBar>();
}
