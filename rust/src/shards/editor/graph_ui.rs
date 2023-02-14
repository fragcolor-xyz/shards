/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2023 Fragcolor Pte. Ltd. */

// Some parts taken from egui_node_graph
// MIT license
// Copyright (c) 2022 setzer22

use egui::*;

pub(crate) trait UIRenderer {
  fn ui(&mut self, ui: &mut Ui) -> Response;
}
