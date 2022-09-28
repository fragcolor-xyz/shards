/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::shardsc::SHColor;
use crate::types::Table;

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

pub(crate) fn get_font_id(size: f32, family: &str) -> Option<egui::FontId> {
  get_font_family(family).map(|family| egui::FontId { family, size })
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
