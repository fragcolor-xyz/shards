/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2023 Fragcolor Pte. Ltd. */

use std::cell::RefCell;
use std::rc::Rc;

use super::egui_host::EguiHost;
use crate::core::registerShard;
use crate::shards::editor::*;
use crate::shardsc::SHTypeInfo;
use crate::types::ExposedTypes;
use crate::types::ParamVar;
use crate::types::Var;
use egui::{Pos2, Vec2};
use slotmap::{SecondaryMap, SlotMap};

struct GraphEditorState<NodeData, NodeTemplate> {
  graph: Graph<NodeData>,
  /// Nodes are drawn in this order. Draw order is important because nodes
  /// that are drawn last are on top.
  node_order: Vec<NodeId>,
  /// The position of each node.
  node_positions: SecondaryMap<NodeId, Pos2>,
  /// The currently selected node. Some interface actions depend on the
  /// currently selected node.
  selected_nodes: Vec<NodeId>,
  /// The mouse drag start position for an ongoing box selection.
  ongoing_box_selection: Option<egui::Pos2>,
  /// The node factory.
  node_factory: Option<NodeFactory<NodeTemplate>>,
  // The node templates.
  all_templates: Vec<NodeTemplate>,
}

struct ShardViewer<'a> {
  parents: ParamVar,
  requiring: ExposedTypes,
  state: GraphEditorState<ShardData<'a>, ShardTemplate<'a>>,
}

// FIXME lots of code shared with UI context as well as Spatial UI
struct WireViewer<'a> {
  wire: ParamVar,
  requiring: ExposedTypes,
  // ---
  // queue: ParamVar,
  graphics_context: ParamVar,
  input_context: ParamVar,
  spatial_context: ParamVar,
  renderer: egui_gfx::Renderer,
  input_translator: egui_gfx::InputTranslator,
  // ---
  graph: Rc<RefCell<Graph<ShardData<'a>>>>,
  panels: SecondaryMap<NodeId, ExtPanel<'a>>,
  // node_hosts: SecondaryMap<NodeId, EguiHost>,
  // node_positions: SecondaryMap<NodeId, [f64; 3]>,
}

struct ExtPanel<'a> {
  host: EguiHost,
  node_id: NodeId,
  graph: Rc<RefCell<Graph<ShardData<'a>>>>,
}

extern "C" {
  fn spatial_getSpatialContextType() -> *mut SHTypeInfo;
  fn spatial_getSpatialContextVarName() -> *const ::std::os::raw::c_char;
  fn spatial_add_panel(ptr: *mut ExtPanel, spatialContextVar: &Var);
}

mod shard_viewer;
mod wire_viewer;

pub fn registerShards() {
  registerShard::<ShardViewer>();
  registerShard::<WireViewer>();
}
