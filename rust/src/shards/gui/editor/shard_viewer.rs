/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2023 Fragcolor Pte. Ltd. */

use super::ShardViewer;
use crate::shard::Shard;
use crate::shards::editor::*;
use crate::shards::gui::util;
use crate::shards::gui::HELP_OUTPUT_EQUAL_INPUT;
use crate::shards::gui::HELP_VALUE_IGNORED;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::types::Context;
use crate::types::ExposedTypes;
use crate::types::OptionalString;
use crate::types::ParamVar;
use crate::types::Types;
use crate::types::Var;
use crate::types::ANY_TYPES;

impl Default for ShardViewer {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      graph: Default::default(),
      node_order: Vec::new(),
      node_positions: Default::default(),
      selected_nodes: Vec::new(),
    }
  }
}

impl Shard for ShardViewer {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.ShardViewer")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.ShardViewer-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.ShardViewer"
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    *HELP_VALUE_IGNORED
  }

  fn outputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    *HELP_OUTPUT_EQUAL_INPUT
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Parents to the list of required variables
    util::require_parents(&mut self.requiring, &self.parents);

    Some(&self.requiring)
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.parents.warmup(context);

    self.init();
    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.parents.cleanup();

    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    if let Some(ui) = util::get_current_parent(self.parents.get())? {
      self.show(ui);

      // Always passthrough the input
      Ok(*input)
    } else {
      Err("No UI parent")
    }
  }
}

impl ShardViewer {
  fn init(&mut self) {}

  fn show(&mut self, ui: &mut egui::Ui) {
    // This causes the graph editor to use as much free space as it can.
    // (so for windows it will use up to the resizeably set limit
    // and for a Panel it will fill it completely)
    let editor_rect = ui.max_rect();
    ui.allocate_rect(editor_rect, egui::Sense::hover());

    // let cursor_pos = ui.ctx().input().pointer.hover_pos().unwrap_or(Pos2::ZERO);
    // let mut cursor_in_editor = editor_rect.contains(cursor_pos);
    // let mut cursor_in_finder = false;

    // The responses returned from node drawing have side effects that are best
    // executed at the end of this function.
    let mut delayed_responses = Vec::new();

    // // Used to detect when the background was clicked
    // let mut click_on_background = false;

    // // Used to detect drag events in the background
    // let mut drag_started_on_background = false;
    // let mut drag_released_on_background = false;

    /* Draw nodes */
    for node_id in self.node_order.iter().copied() {
      let responses = GraphNodeWidget {
        position: self.node_positions.get_mut(node_id).unwrap(),
        graph: &mut self.graph,
        node_id,
        pan: editor_rect.min.to_vec2(),
        selected: self
          .selected_nodes
          .iter()
          .any(|selected| *selected == node_id),
      }
      .show(ui);

      delayed_responses.extend(responses);
    }

    // let r = ui.allocate_rect(
    //   ui.min_rect(),
    //   egui::Sense::click().union(egui::Sense::drag()),
    // );
    // if r.clicked() {
    //   click_on_background = true;
    // } else if r.drag_started() {
    //   drag_started_on_background = true;
    // } else if r.drag_released() {
    //   drag_released_on_background = true;
    // }

    /* Handle responses from drawing nodes */
    for response in delayed_responses.iter() {
      match response {
        NodeResponse::MoveNode { node, drag_delta } => {
          self.node_positions[*node] += *drag_delta;
        }
        NodeResponse::RaiseNode(node_id) => {
          let old_pos = self
            .node_order
            .iter()
            .position(|id| *id == *node_id)
            .expect("Node to be raised should be in `node_order`");
          self.node_order.remove(old_pos);
          self.node_order.push(*node_id);
        }
        NodeResponse::SelectNode(node_id) => {
          self.selected_nodes = Vec::from([*node_id]);
        }
      }
    }
  }
}
