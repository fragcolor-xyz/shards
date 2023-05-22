/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use shards::shardsc::SHColor;
use shards::types::Table;

pub(crate) fn get_color(color: SHColor) -> egui::Color32 {
  egui::Color32::from_rgba_unmultiplied(color.r, color.g, color.b, color.a)
}

pub(crate) fn get_font_family(family: &str) -> Option<egui::FontFamily> {
  match family {
    "Proportional" => Some(egui::FontFamily::Proportional),
    "Monospace" => Some(egui::FontFamily::Monospace),
    "" => None,
    name => Some(egui::FontFamily::Name(name.into())),
  }
}

pub(crate) fn get_font_id(font_id: Table) -> Option<egui::FontId> {
  if let (Some(size), Some(family)) = (
    font_id.get_static(cstr!("size")),
    font_id.get_static(cstr!("family")),
  ) {
    let size: f32 = size.try_into().unwrap_or_default();
    let family: &str = family.try_into().unwrap_or_default();
    get_font_family(family).map(|family| egui::FontId { family, size })
  } else {
    None
  }
}

pub(crate) fn get_margin(margin: (f32, f32, f32, f32)) -> egui::style::Margin {
  egui::style::Margin {
    left: margin.0,
    right: margin.1,
    top: margin.2,
    bottom: margin.3,
  }
}

pub(crate) fn get_rounding(rounding: (f32, f32, f32, f32)) -> egui::Rounding {
  egui::Rounding {
    nw: rounding.0,
    ne: rounding.1,
    sw: rounding.2,
    se: rounding.3,
  }
}

pub(crate) fn get_shadow(shadow: Table) -> egui::epaint::Shadow {
  if let (Some(extrusion), Some(color)) = (
    shadow.get_static(cstr!("extrusion")),
    shadow.get_static(cstr!("color")),
  ) {
    egui::epaint::Shadow {
      extrusion: extrusion.try_into().unwrap_or_default(),
      color: get_color(color.try_into().unwrap_or_default()),
    }
  } else {
    Default::default()
  }
}

pub(crate) fn get_stroke(stroke: Table) -> egui::Stroke {
  if let (Some(width), Some(color)) = (
    stroke.get_static(cstr!("width")),
    stroke.get_static(cstr!("color")),
  ) {
    egui::Stroke {
      width: width.try_into().unwrap_or_default(),
      color: get_color(color.try_into().unwrap_or_default()),
    }
  } else {
    Default::default()
  }
}

pub(crate) fn get_text_format(format: Table) -> egui::TextFormat {
  let mut text_format = egui::TextFormat::default();
  update_text_format(&mut text_format, format);
  text_format
}

pub(crate) fn get_text_style(name: &str) -> Option<egui::TextStyle> {
  match name {
    "Small" => Some(egui::TextStyle::Small),
    "Body" => Some(egui::TextStyle::Body),
    "Monospace" => Some(egui::TextStyle::Monospace),
    "Button" => Some(egui::TextStyle::Button),
    "Heading" => Some(egui::TextStyle::Heading),
    "" => None,
    name => Some(egui::TextStyle::Name(name.into())),
  }
}

pub(crate) fn update_text_format(text_format: &mut egui::TextFormat, format: Table) {
  if let Some(font_id) = format.get_static(cstr!("font_id")) {
    let font_id: Table = font_id.try_into().unwrap_or_default();
    if let Some(font_id) = get_font_id(font_id) {
      text_format.font_id = font_id;
    }
  }

  if let Some(color) = format.get_static(cstr!("color")) {
    let color: SHColor = color.try_into().unwrap_or_default();
    text_format.color = get_color(color);
  }

  if let Some(background) = format.get_static(cstr!("background")) {
    let background: SHColor = background.try_into().unwrap_or_default();
    text_format.background =
      egui::Color32::from_rgba_unmultiplied(background.r, background.g, background.b, background.a);
  }

  if let Some(italics) = format.get_static(cstr!("italics")) {
    text_format.italics = italics.try_into().unwrap_or_default();
  }

  if let Some(underline) = format.get_static(cstr!("underline")) {
    let underline: Table = underline.try_into().unwrap_or_default();
    text_format.underline = get_stroke(underline);
  }

  if let Some(strikethrough) = format.get_static(cstr!("strikethrough")) {
    let strikethrough: Table = strikethrough.try_into().unwrap_or_default();
    text_format.strikethrough = get_stroke(strikethrough);
  }
}
