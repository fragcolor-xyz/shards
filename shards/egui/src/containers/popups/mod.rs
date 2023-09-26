/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */
use shards::core::register_enum;

#[derive(shards::shards_enum)]
#[enum_info(
  b"egPL",
  "PopupLocation",
  "Determines the location of the popup relative to the widget that it is attached to."
)]
pub enum PopupLocation {
  #[enum_value("Below.")]
  Below = 0,
  #[enum_value("Above.")]
  Above = 1,
}

impl From<PopupLocation> for egui::AboveOrBelow {
  fn from(popup_location: PopupLocation) -> Self {
    match popup_location {
      PopupLocation::Above => egui::AboveOrBelow::Above,
      PopupLocation::Below => egui::AboveOrBelow::Below,
      _ => unreachable!(),
    }
  }
}

mod popup_button;
mod popup_image_button;

pub fn register_shards() {
  register_enum::<PopupLocation>();
  popup_button::register_shards();
  popup_image_button::register_shards();
}
