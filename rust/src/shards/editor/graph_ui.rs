/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2023 Fragcolor Pte. Ltd. */

// Some parts taken from egui_node_graph
// MIT license
// Copyright (c) 2022 setzer22

use super::graph::*;
use super::id_types::*;
use super::util::ColorUtils;
use egui::{
  epaint::RectShape, style::Margin, Area, Color32, Frame, Id, InnerResponse, Order, Pos2, Rect,
  Response, Rounding, Sense, Shape, Stroke, Ui, Vec2,
};
use std::collections::HashMap;

pub(crate) struct GraphNodeWidget<'a, NodeData> {
  pub position: &'a mut Pos2,
  pub graph: &'a mut Graph<NodeData>,
  pub node_rects: &'a mut HashMap<NodeId, Rect>,
  pub node_id: NodeId,
  pub pan: Vec2,
  pub selected: bool,
}

impl<'a, NodeData> GraphNodeWidget<'a, NodeData>
where
  NodeData: UIRenderer,
{
  pub fn show(self, ui: &mut Ui) -> Vec<NodeResponse> {
    let node_id = self.node_id;
    let area = Area::new(Id::new(node_id))
      .order(Order::Middle)
      .constrain(false) // FIXME egui BUG, this is not taken into account if the Area is movable
      .current_pos(*self.position + self.pan);

    let InnerResponse {
      inner: mut node_responses,
      response,
    } = area.show(ui.ctx(), |ui| self.show_graph_node(ui));

    // Movement
    let drag_delta = response.drag_delta();
    if drag_delta.length_sq() > 0.0 {
      node_responses.push(NodeResponse::MoveNode {
        node_id,
        drag_delta,
      });
      node_responses.push(NodeResponse::RaiseNode(node_id));
    }

    // Node selection
    //
    // HACK: Only set the select response when no other response is active.
    // This prevents some issues.
    if node_responses.is_empty() && response.clicked_by(egui::PointerButton::Primary) {
      node_responses.push(NodeResponse::SelectNode(node_id));
      node_responses.push(NodeResponse::RaiseNode(node_id));
    }

    node_responses
  }

  fn show_graph_node(self, ui: &mut Ui) -> Vec<NodeResponse> {
    let margin = Margin::symmetric(15.0, 5.0);
    let mut responses = Vec::new();

    let (background_color, text_color) = if ui.visuals().dark_mode {
      (
        Color32::from_hex("#3f3f3f").unwrap(),
        Color32::from_hex("#fefefe").unwrap(),
      )
    } else {
      (
        Color32::from_hex("#ffffff").unwrap(),
        Color32::from_hex("#505050").unwrap(),
      )
    };

    ui.visuals_mut().widgets.noninteractive.fg_stroke = egui::Stroke::new(2.0, text_color);

    // Preallocate shapes to paint below contents
    let outline_shape = ui.painter().add(egui::Shape::Noop);
    let background_shape = ui.painter().add(egui::Shape::Noop);

    let mut title_height = 0.0;

    let node = &mut self.graph[self.node_id];

    Frame::none().inner_margin(margin).show(ui, |ui| {
      ui.vertical(|ui| {
        ui.horizontal(|ui| {
          ui.add(egui::Label::new(
            egui::RichText::new(&node.label)
              .text_style(egui::TextStyle::Button)
              .color(text_color),
          ));
          ui.add_space(8.0); // The size of the little cross icon
        });
        title_height = ui.min_size().y;
        ui.add_space(margin.top);

        // First pass: Draw the inner fields. Compute port heights
        node.data.ui(ui);
      })
    });

    // Second pass, iterate again to draw the ports. This happens outside
    // the child_ui because we want ports to overflow the node background.
    let outer_rect = ui.min_rect();

    // Draw the background shape.
    // NOTE: This code is a bit more involved than it needs to be because egui
    // does not support drawing rectangles with asymmetrical round corners.
    let (shape, outline) = {
      let rounding_radius = 4.0;

      let titlebar_height = title_height + margin.top * 2.0;
      let titlebar_rect = Rect::from_min_size(
        outer_rect.min,
        egui::vec2(outer_rect.width(), titlebar_height),
      );
      let titlebar = egui::Shape::Rect(egui::epaint::RectShape {
        rect: titlebar_rect,
        rounding: Rounding {
          nw: rounding_radius,
          ne: rounding_radius,
          sw: 0.0,
          se: 0.0,
        },
        fill: background_color.lighten(0.8),
        stroke: egui::Stroke::NONE,
      });

      let body_rect = Rect::from_min_size(
        outer_rect.min + egui::vec2(0.0, titlebar_height),
        egui::vec2(outer_rect.width(), outer_rect.height() - titlebar_height),
      );
      let body = Shape::Rect(RectShape {
        rect: body_rect,
        rounding: Rounding {
          nw: 0.0,
          ne: 0.0,
          sw: rounding_radius,
          se: rounding_radius,
        },
        fill: background_color,
        stroke: egui::Stroke::NONE,
      });

      let node_rect = titlebar_rect.union(body_rect);
      let outline = if self.selected {
        egui::Shape::Rect(egui::epaint::RectShape {
          rect: node_rect.expand(1.0),
          rounding: Rounding::same(rounding_radius),
          fill: Color32::WHITE.lighten(0.8),
          stroke: egui::Stroke::NONE,
        })
      } else {
        egui::Shape::Noop
      };

      // Take note of the node rect, so the editor can use it later to compute intersections.
      self.node_rects.insert(self.node_id, node_rect);

      (egui::Shape::Vec(vec![titlebar, body]), outline)
    };

    ui.painter().set(background_shape, shape);
    ui.painter().set(outline_shape, outline);

    // --- Interaction ---
    if Self::close_button(ui, outer_rect).clicked() {
      responses.push(NodeResponse::DeleteNodeUi(self.node_id));
    }

    responses
  }

  fn close_button(ui: &mut Ui, node_rect: Rect) -> Response {
    // Measurements
    let margin = 8.0;
    let size = 10.0;
    let stroke_width = 2.0;
    let offs = margin + size / 2.0;

    let position = Pos2 {
      x: node_rect.right() - offs,
      y: node_rect.top() + offs,
    };
    let rect = Rect::from_center_size(position, Vec2 { x: size, y: size });
    let resp = ui.allocate_rect(rect, Sense::click());

    let dark_mode = ui.visuals().dark_mode;
    let color = if resp.clicked() {
      if dark_mode {
        Color32::from_hex("#ffffff").unwrap()
      } else {
        Color32::from_hex("#000000").unwrap()
      }
    } else if resp.hovered() {
      if dark_mode {
        Color32::from_hex("#dddddd").unwrap()
      } else {
        Color32::from_hex("#222222").unwrap()
      }
    } else {
      if dark_mode {
        Color32::from_hex("#aaaaaa").unwrap()
      } else {
        Color32::from_hex("#555555").unwrap()
      }
    };
    let stroke = Stroke {
      width: stroke_width,
      color,
    };

    ui.painter()
      .line_segment([rect.left_top(), rect.right_bottom()], stroke);
    ui.painter()
      .line_segment([rect.right_top(), rect.left_bottom()], stroke);

    resp
  }
}

pub(crate) enum NodeResponse {
  DeleteNodeUi(NodeId),
  MoveNode {
    node_id: NodeId,
    drag_delta: Vec2,
  },
  /// Emitted when a node is interacted with, and should be raised
  RaiseNode(NodeId),
  SelectNode(NodeId),
}

pub(crate) trait UIRenderer {
  fn ui(&mut self, ui: &mut Ui) -> Response;
}
