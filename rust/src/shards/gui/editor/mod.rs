/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2023 Fragcolor Pte. Ltd. */

use crate::core::registerShard;
use crate::shards::editor::*;
use crate::types::ExposedTypes;
use crate::types::ParamVar;
use egui::{Pos2, Vec2};
use slotmap::{SecondaryMap, SlotMap};

struct ShardViewer {
  parents: ParamVar,
  requiring: ExposedTypes,
  graph: Graph<NodeTemplate>,
  /// Nodes are drawn in this order. Draw order is important because nodes
  /// that are drawn last are on top.
  node_order: Vec<NodeId>,
  /// The position of each node.
  node_positions: SecondaryMap<NodeId, Pos2>,
  /// The currently selected node. Some interface actions depend on the
  /// currently selected node.
  selected_nodes: Vec<NodeId>,
}

mod shard_viewer;

pub fn registerShards() {
  registerShard::<ShardViewer>();
}
