/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use shards::shardsc::SHColor;
use shards::types::Table;

pub(crate) fn get_color(color: SHColor) -> Result<egui::Color32, &'static str> {
  Ok(egui::Color32::from_rgba_unmultiplied(
    color.r, color.g, color.b, color.a,
  ))
}

pub(crate) fn get_font_family(family: &str) -> Result<egui::FontFamily, &'static str> {
  match family {
    "Proportional" => Ok(egui::FontFamily::Proportional),
    "Monospace" => Ok(egui::FontFamily::Monospace),
    "" => Err("No font family provided"),
    name => Ok(egui::FontFamily::Name(name.into())), // TODO: Check if this is ok or how to return Err if cannot construct
  }
}

pub(crate) fn get_font_id(font_id: Table) -> Result<egui::FontId, &'static str> {
  if let (Some(size), Some(family)) = (font_id.get_static("size"), font_id.get_static("family")) {
    let size: f32 = size.try_into().map_err(|e| {
      shlog!("{}", e);
      "Invalid value received for size attribute"
    })?;
    let family: &str = family.try_into().map_err(|e| {
      shlog!("{}", e);
      "Invalid value received for family attribute"
    })?;
    get_font_family(family).map(|family| egui::FontId { family, size })
  } else {
    Err("Font size or family not found in text style provided")
  }
}

pub(crate) fn get_margin(
  margin: (f32, f32, f32, f32),
) -> Result<egui::style::Margin, &'static str> {
  Ok(egui::style::Margin {
    left: margin.0,
    right: margin.1,
    top: margin.2,
    bottom: margin.3,
  })
}

pub(crate) fn get_rounding(rounding: (f32, f32, f32, f32)) -> Result<egui::Rounding, &'static str> {
  Ok(egui::Rounding {
    nw: rounding.0,
    ne: rounding.1,
    sw: rounding.2,
    se: rounding.3,
  })
}

pub(crate) fn get_shadow(shadow: Table) -> Result<egui::epaint::Shadow, &'static str> {
  if let (Some(extrusion), Some(color)) =
    (shadow.get_static("extrusion"), shadow.get_static("color"))
  {
    Ok(egui::epaint::Shadow {
      extrusion: extrusion.try_into().map_err(|e| {
        shlog!("{}", e);
        "Invalid value received for extrusion attribute"
      })?,
      color: get_color(color.try_into().map_err(|e| {
        shlog!("{}", e);
        "Invalid value received for color attribute"
      })?)?,
    })
  } else {
    Err("Shadow extrusion or color not found in table provided")
  }
}

pub(crate) fn get_stroke(stroke: Table) -> Result<egui::Stroke, &'static str> {
  if let (Some(width), Some(color)) = (stroke.get_static("width"), stroke.get_static("color")) {
    Ok(egui::Stroke {
      width: width.try_into().map_err(|e| {
        shlog!("{}", e);
        "Invalid value received for width attribute"
      })?,
      color: get_color(color.try_into().map_err(|e| {
        shlog!("{}", e);
        "Invalid value received for color attribute"
      })?)?,
    })
  } else {
    Err("Stroke width or color not found in table provided")
  }
}

pub(crate) fn get_text_format(format: Table) -> Result<egui::TextFormat, &'static str> {
  let mut text_format = egui::TextFormat::default();
  update_text_format(&mut text_format, format)?;
  Ok(text_format)
}

pub(crate) fn get_text_style(name: &str) -> Result<egui::TextStyle, &'static str> {
  match name {
    "Small" => Ok(egui::TextStyle::Small),
    "Body" => Ok(egui::TextStyle::Body),
    "Monospace" => Ok(egui::TextStyle::Monospace),
    "Button" => Ok(egui::TextStyle::Button),
    "Heading" => Ok(egui::TextStyle::Heading),
    "" => Err("No text style provided"),
    name => Ok(egui::TextStyle::Name(name.into())), // TODO: Check if this is ok or how to return Err if cannot construct
  }
}

pub(crate) fn update_text_format(
  text_format: &mut egui::TextFormat,
  format: Table,
) -> Result<(), &'static str> {
  if let Some(font_id) = format.get_static("font_id") {
    let font_id: Table = font_id.try_into().map_err(|e| {
      shlog!("{}", e);
      "Invalid value received for font_id attribute"
    })?;
    text_format.font_id = get_font_id(font_id)?;
  }

  if let Some(color) = format.get_static("color") {
    let color: SHColor = color.try_into().map_err(|e| {
      shlog!("{}", e);
      "Invalid value received for color attribute"
    })?;
    text_format.color = get_color(color)?;
  }

  if let Some(background) = format.get_static("background") {
    let background: SHColor = background.try_into().map_err(|e| {
      shlog!("{}", e);
      "Invalid value received for background attribute"
    })?;
    text_format.background =
      egui::Color32::from_rgba_unmultiplied(background.r, background.g, background.b, background.a);
  }

  if let Some(italics) = format.get_static("italics") {
    text_format.italics = italics.try_into().map_err(|e| {
      shlog!("{}", e);
      "Invalid value received for italics attribute"
    })?;
  }

  if let Some(underline) = format.get_static("underline") {
    let underline: Table = underline.try_into().map_err(|e| {
      shlog!("{}", e);
      "Invalid value received for underline attribute"
    })?;
    text_format.underline = get_stroke(underline)?;
  }

  if let Some(strikethrough) = format.get_static("strikethrough") {
    let strikethrough: Table = strikethrough.try_into().map_err(|e| {
      shlog!("{}", e);
      "Invalid value received for strikethrough attribute"
    })?;
    text_format.strikethrough = get_stroke(strikethrough)?;
  }

  Ok(())
}
