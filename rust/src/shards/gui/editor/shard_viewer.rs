/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2023 Fragcolor Pte. Ltd. */

use super::GraphEditorState;
use super::ShardViewer;
use crate::core::getShards;
use crate::shard::Shard;
use crate::shards::editor::*;
use crate::shards::gui::util;
use crate::shards::gui::HELP_OUTPUT_EQUAL_INPUT;
use crate::shards::gui::HELP_VALUE_IGNORED;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::shardsc::*;
use crate::types::Context;
use crate::types::ExposedTypes;
use crate::types::OptionalString;
use crate::types::ParamVar;
use crate::types::Types;
use crate::types::Var;
use crate::types::ANY_TYPES;
use std::collections::HashMap;

impl<NodeData, NodeTemplate> Default for GraphEditorState<NodeData, NodeTemplate> {
  fn default() -> Self {
    Self {
      graph: Default::default(),
      node_order: Vec::new(),
      node_positions: Default::default(),
      selected_nodes: Vec::new(),
      ongoing_box_selection: None,
      node_factory: None,
      all_templates: Vec::new(),
    }
  }
}

impl Default for ShardViewer {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      state: Default::default(),
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

// Euler problem 1
// ---------------
// required "blocks":
// - wire definition
// - variable definition
// - integer constant
// - boolean constant
// - Assert.Is
// - Get
// - ForRange
// - Is
// - Log
// - Or
// - Math.Add
// - Math.Mod
// - Set
// - When
//
// notes: for now, mesh, scheduling and running will be implicit/hidden
//
// shard code:
// (defwire main-wire
//   0 >= .sum
//
//   (ForRange
//    1 999
//    (When
//     (-> (Math.Mod 3) (Is 0) (Or) (Math.Mod 5) (Is 0))
//     (-> (Math.Add .sum) > .sum)))
//
//   .sum (Assert.Is 233168 true)
//   (Log "Answer"))
//
// (defmesh root)
// (schedule root main-wire)
// (run root)

impl ShardViewer {
  fn init(&mut self) {
    let mut shards: Vec<&str> = getShards()
      .into_iter()
      .map(|s| s.to_str().unwrap())
      .collect();
    shards.sort();
    let templates = shards.iter().map(|s| ShardTemplate::new(s));
    self.state.all_templates.extend(templates);
  }

  fn show(&mut self, ui: &mut egui::Ui) {
    // This causes the graph editor to use as much free space as it can.
    // (so for windows it will use up to the resizeably set limit
    // and for a Panel it will fill it completely)
    let editor_rect = ui.max_rect();
    ui.allocate_rect(editor_rect, egui::Sense::hover());

    let cursor_pos = ui
      .ctx()
      .input()
      .pointer
      .hover_pos()
      .unwrap_or(egui::Pos2::ZERO);
    let mut cursor_in_editor = editor_rect.contains(cursor_pos);
    let mut cursor_in_factory = false;

    let mut node_rects = HashMap::<NodeId, egui::Rect>::new();

    // The responses returned from node drawing have side effects that are best
    // executed at the end of this function.
    let mut delayed_responses = Vec::new();

    // // Used to detect when the background was clicked
    let mut click_on_background = false;

    // // Used to detect drag events in the background
    let mut drag_started_on_background = false;
    let mut drag_released_on_background = false;

    /* Draw nodes */
    for node_id in self.state.node_order.iter().copied() {
      let responses = GraphNodeWidget {
        position: self.state.node_positions.get_mut(node_id).unwrap(),
        graph: &mut self.state.graph,
        node_rects: &mut node_rects,
        node_id,
        pan: editor_rect.min.to_vec2(),
        selected: self
          .state
          .selected_nodes
          .iter()
          .any(|selected| *selected == node_id),
      }
      .show(ui);

      delayed_responses.extend(responses);
    }

    let r = ui.allocate_rect(
      ui.min_rect(),
      egui::Sense::click().union(egui::Sense::drag()),
    );
    if r.clicked() {
      click_on_background = true;
    } else if r.drag_started() {
      drag_started_on_background = true;
    } else if r.drag_released() {
      drag_released_on_background = true;
    }

    /* Draw the node factory, if open */
    let mut should_close_node_factory = false;
    if let Some(ref mut node_factory) = self.state.node_factory {
      let mut node_factory_area = egui::Area::new("node_factory")
        .order(egui::Order::Foreground)
        .movable(false);
      if let Some(pos) = node_factory.position {
        node_factory_area = node_factory_area.current_pos(pos);
      }
      node_factory_area.show(ui.ctx(), |ui| {
        if let Some(template) = node_factory.show(ui, &self.state.all_templates) {
          let new_node = self
            .state
            .graph
            .add_node(template.node_factory_label().into(), template.build_node());
          self.state.node_positions.insert(
            new_node,
            node_factory.position.unwrap_or(cursor_pos) - editor_rect.min.to_vec2(),
          );
          self.state.node_order.push(new_node);
          should_close_node_factory = true;
        }
        let factory_rect = ui.min_rect();
        // If the cursor is not in the main editor, check if the cursor is in the factory
        // if the cursor is in the factory, then we can consider that also in the editor.
        if factory_rect.contains(cursor_pos) {
          cursor_in_editor = true;
          cursor_in_factory = true;
        }
      });
    }
    if should_close_node_factory {
      self.state.node_factory = None;
    }

    /* Handle responses from drawing nodes */
    for response in delayed_responses.iter() {
      match response {
        NodeResponse::DeleteNodeUi(node_id) => {
          self.state.graph.remove_node(*node_id);
          self.state.node_positions.remove(*node_id);
          // Make sure to not leave references to old nodes hanging
          self.state.selected_nodes.retain(|id| *id != *node_id);
          self.state.node_order.retain(|id| *id != *node_id);
        }
        NodeResponse::MoveNode {
          node_id,
          drag_delta,
        } => {
          self.state.node_positions[*node_id] += *drag_delta;
          // Handle multi-node selection movement
          if self.state.selected_nodes.contains(node_id) && self.state.selected_nodes.len() > 1 {
            for n in self.state.selected_nodes.iter().copied() {
              if n != *node_id {
                self.state.node_positions[n] += *drag_delta;
              }
            }
          }
        }
        NodeResponse::RaiseNode(node_id) => {
          let old_pos = self
            .state
            .node_order
            .iter()
            .position(|id| *id == *node_id)
            .expect("Node to be raised should be in `node_order`");
          self.state.node_order.remove(old_pos);
          self.state.node_order.push(*node_id);
        }
        NodeResponse::SelectNode(node_id) => {
          self.state.selected_nodes = Vec::from([*node_id]);
        }
      }
    }

    // Box selection
    if let Some(box_start) = self.state.ongoing_box_selection {
      let selection_rect = egui::Rect::from_two_pos(cursor_pos, box_start);
      let bg_color = egui::Color32::from_rgba_unmultiplied(200, 200, 200, 20);
      let stroke_color = egui::Color32::from_rgba_unmultiplied(200, 200, 200, 180);
      ui.painter().rect(
        selection_rect,
        2.0,
        bg_color,
        egui::Stroke::new(3.0, stroke_color),
      );

      self.state.selected_nodes = node_rects
        .into_iter()
        .filter_map(|(node_id, rect)| {
          if selection_rect.intersects(rect) {
            Some(node_id)
          } else {
            None
          }
        })
        .collect();
    }

    /* Mouse input handling */
    let mouse = &ui.ctx().input().pointer;
    if mouse.secondary_released() && cursor_in_editor && !cursor_in_factory {
      self.state.node_factory = Some(NodeFactory::new_at(cursor_pos));
    }
    if ui.ctx().input().key_pressed(egui::Key::Escape) {
      self.state.node_factory = None;
    }
    // Deselect and deactivate factory if the editor backround is clicked,
    // *or* if the the mouse clicks off the ui
    if click_on_background || (mouse.any_click() && !cursor_in_editor) {
      self.state.selected_nodes = Vec::new();
      self.state.node_factory = None;
    }

    if drag_started_on_background && mouse.primary_down() {
      self.state.ongoing_box_selection = Some(cursor_pos);
    }
    if mouse.primary_released() || drag_released_on_background {
      self.state.ongoing_box_selection = None;
    }
  }
}
