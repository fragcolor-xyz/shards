/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2023 Fragcolor Pte. Ltd. */

use crate::core::registerShard;
use crate::shards::editor::Graph;
use crate::shards::editor::NodeId;
use crate::shards::editor::ShardData;
use crate::types::ExposedTypes;
use crate::types::ParamVar;

struct WireViewer<'a> {
  wire: ParamVar,
  parents: ParamVar,
  requiring: ExposedTypes,
  graph: Graph<ShardData<'a>>,
  node_order: Vec<NodeId>,
}

mod wire_viewer;

pub fn registerShards() {
  registerShard::<WireViewer>();
}
