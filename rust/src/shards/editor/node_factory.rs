/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2023 Fragcolor Pte. Ltd. */

use super::util::ColorUtils;
use egui::{Color32, Frame, Key, Pos2, ScrollArea, Stroke, Ui, Vec2};
use std::marker::PhantomData;

pub(crate) struct NodeFactory<NodeData> {
  pub query: String,
  pub position: Option<Pos2>,
  _phantom: PhantomData<NodeData>,
}

impl<NodeData> NodeFactory<NodeData>
where
  NodeData: NodeTemplateTrait,
{
  pub fn new_at(pos: Pos2) -> Self {
    Self {
      query: "".into(),
      position: Some(pos),
      _phantom: Default::default(),
    }
  }

  pub fn show(&mut self, ui: &mut Ui, templates: &Vec<NodeData>) -> Option<NodeData> {
    let (background_color, text_color) = if ui.visuals().dark_mode {
      (
        Color32::from_hex("#3f3f3f").unwrap(),
        Color32::from_hex("#fefefe").unwrap(),
      )
    } else {
      (
        Color32::from_hex("#fefefe").unwrap(),
        Color32::from_hex("#3f3f3f").unwrap(),
      )
    };

    ui.visuals_mut().widgets.noninteractive.fg_stroke = Stroke::new(2.0, text_color);

    let mut ret = None;
    Frame::dark_canvas(ui.style())
      .fill(background_color)
      .inner_margin(Vec2 { x: 5.0, y: 5.0 })
      .show(ui, |ui| {
        let mut description = "";
        let response = ui.text_edit_singleline(&mut self.query);
        let mut query_submit = response.lost_focus() && ui.input().key_down(Key::Enter);
        let max_height = ui.input().screen_rect.height() * 0.5;
        let scroll_area_width = response.rect.width() - 30.0;
        Frame::default().show(ui, |ui| {
          ScrollArea::vertical()
            .max_height(max_height)
            .show(ui, |ui| {
              ui.set_width(scroll_area_width);
              for template in templates.into_iter() {
                let label = template.node_factory_label();
                if label.to_lowercase().contains(&self.query.to_lowercase()) {
                  let response = ui.selectable_label(false, label);
                  if response.hovered() {
                    description = template.node_factory_description();
                  }
                  if response.clicked() {
                    ret = Some(template.clone());
                  } else if query_submit {
                    ret = Some(template.clone());
                    query_submit = false;
                  }
                }
              }
            });
        });
        ui.separator();
        ui.label(description);
      });

    ret
  }
}

pub(crate) trait NodeTemplateTrait: Clone {
  fn node_factory_label(&self) -> &str;

  fn node_factory_description(&self) -> &str {
    self.node_factory_label()
  }
}

// FIXME: need a display name for each template
// TODO: might also add a description that be shown below (for instance we could take the result of the help() function on a shard.)
