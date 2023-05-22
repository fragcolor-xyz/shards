/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::misc::style_util;
use shards::shardsc::SHColor;
use shards::types::Table;

pub(crate) fn get_styled_text(
  text: egui::RichText,
  style: &Table,
) -> Result<egui::RichText, &'static str> {
  let mut text = text;

  if let Some(text_style) = style.get_static(cstr!("text_style")) {
    if !text_style.is_none() {
      let text_style: &str = text_style.try_into()?;
      if let Some(text_style) = style_util::get_text_style(text_style) {
        text = text.text_style(text_style);
      }
    }
  }

  if let Some(color) = style.get_static(cstr!("color")) {
    if !color.is_none() {
      let color: SHColor = color.try_into()?;
      text = text.color(egui::Color32::from_rgba_unmultiplied(
        color.r, color.g, color.b, color.a,
      ));
    }
  }

  if let Some(italics) = style.get_static(cstr!("italics")) {
    if !italics.is_none() && italics.try_into()? {
      text = text.italics();
    }
  }

  if let Some(size) = style.get_static(cstr!("size")) {
    if !size.is_none() {
      text = text.size(size.try_into()?);
    }
  }

  if let Some(strikethrough) = style.get_static(cstr!("strikethrough")) {
    if !strikethrough.is_none() && strikethrough.try_into()? {
      text = text.strikethrough();
    }
  }

  if let Some(underline) = style.get_static(cstr!("underline")) {
    if !underline.is_none() && underline.try_into()? {
      text = text.underline();
    }
  }

  Ok(text)
}
