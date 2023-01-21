/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2023 Fragcolor Pte. Ltd. */

use super::graph_ui::UIRenderer;
use egui::Ui;

pub(crate) enum NodeTemplate {}

impl UIRenderer for NodeTemplate {
  fn ui(&mut self, ui: &mut Ui) {
    match self {
      _ => {}
    }
  }
}
