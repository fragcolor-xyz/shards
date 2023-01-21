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
      node_factory: None,
      all_templates: Vec::new(),
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
    let templates = vec![
      NodeTemplate::AssertIs(Default::default()),
      NodeTemplate::Const(Default::default()),
      NodeTemplate::ForRange(Default::default()),
      NodeTemplate::Get(Default::default()),
      NodeTemplate::Log(Default::default()),
      NodeTemplate::MathAdd(Default::default()),
      NodeTemplate::MathDivide(Default::default()),
      NodeTemplate::MathMultiply(Default::default()),
      NodeTemplate::MathMod(Default::default()),
      NodeTemplate::MathSubtract(Default::default()),
      NodeTemplate::Set(Default::default()),
    ];
    self.all_templates.extend(templates);
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

    // The responses returned from node drawing have side effects that are best
    // executed at the end of this function.
    let mut delayed_responses = Vec::new();

    // // Used to detect when the background was clicked
    let mut click_on_background = false;

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

    let r = ui.allocate_rect(
      ui.min_rect(),
      egui::Sense::click().union(egui::Sense::drag()),
    );
    if r.clicked() {
      click_on_background = true;
      // } else if r.drag_started() {
      //   drag_started_on_background = true;
      // } else if r.drag_released() {
      //   drag_released_on_background = true;
    }

    /* Draw the node factory, if open */
    let mut should_close_node_factory = false;
    if let Some(ref mut node_factory) = self.node_factory {
      let mut node_factory_area = egui::Area::new("node_factory")
        .order(egui::Order::Foreground)
        .movable(false);
      if let Some(pos) = node_factory.position {
        node_factory_area = node_factory_area.current_pos(pos);
      }
      node_factory_area.show(ui.ctx(), |ui| {
        if let Some(template) = node_factory.show(ui, &self.all_templates) {
          let new_node = self
            .graph
            .add_node(template.node_factory_label().into(), template.clone());
          self
            .node_positions
            .insert(new_node, cursor_pos - editor_rect.min.to_vec2());
          self.node_order.push(new_node);
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
      self.node_factory = None;
    }

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

    /* Mouse input handling */
    let mouse = &ui.ctx().input().pointer;
    if mouse.secondary_released() && cursor_in_editor && !cursor_in_factory {
      self.node_factory = Some(NodeFactory::new_at(cursor_pos));
    }
    if ui.ctx().input().key_pressed(egui::Key::Escape) {
      self.node_factory = None;
    }
    // Deselect and deactivate factory if the editor backround is clicked,
    // *or* if the the mouse clicks off the ui
    if click_on_background || (mouse.any_click() && !cursor_in_editor) {
      self.selected_nodes = Vec::new();
      self.node_factory = None;
    }
  }
}
