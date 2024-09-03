/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::misc::style_util;
use shards::shardsc::SHColor;
use shards::types::Table;

pub fn get_styled_text(
  text: egui::RichText,
  style: &Table,
) -> Result<egui::RichText, &'static str> {
  let mut text = text;

  if let Some(text_style) = style.get_static("text_style") {
    if !text_style.is_none() {
      let text_style: &str = text_style.try_into()?;
      text = text.text_style(style_util::get_text_style(text_style)?);
    }
  }

  if let Some(color) = style.get_static("color") {
    if !color.is_none() {
      let color: SHColor = color.try_into()?;
      text = text.color(egui::Color32::from_rgba_unmultiplied(
        color.r, color.g, color.b, color.a,
      ));
    }
  }

  if let Some(italics) = style.get_static("italics") {
    if !italics.is_none() && italics.try_into()? {
      text = text.italics();
    }
  }

  if let Some(size) = style.get_static("size") {
    if !size.is_none() {
      text = text.size(size.try_into()?);
    }
  }

  if let Some(strikethrough) = style.get_static("strikethrough") {
    if !strikethrough.is_none() && strikethrough.try_into()? {
      text = text.strikethrough();
    }
  }

  if let Some(underline) = style.get_static("underline") {
    if !underline.is_none() && underline.try_into()? {
      text = text.underline();
    }
  }

  if let Some(weak) = style.get_static("weak") {
    if !weak.is_none() && weak.try_into()? {
      text = text.weak();
    }
  }

  if let Some(strong) = style.get_static("strong") {
    if !strong.is_none() && strong.try_into()? {
      text = text.strong();
    }
  }

  Ok(text)
}
