/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2023 Fragcolor Pte. Ltd. */

// Some parts taken from egui_node_graph
// MIT license
// Copyright (c) 2022 setzer22

use egui::Color32;

pub(crate) trait ColorUtils: Sized {
  type Error;

  /// Multiplies the color rgb values by `factor`, keeping alpha untouched.
  fn lighten(&self, factor: f32) -> Self;
  /// Converts a hex string with a leading '#' into a egui::Color32.
  /// - The first three channels are interpreted as R, G, B.
  /// - The fourth channel, if present, is used as the alpha value.
  /// - Both upper and lowercase characters can be used for the hex values.
  ///
  /// *Adapted from: https://docs.rs/raster/0.1.0/src/raster/lib.rs.html#425-725.
  /// Credit goes to original authors.*
  fn from_hex(hex: &str) -> Result<Self, Self::Error>;
  /// Converts a color into its canonical hexadecimal representation.
  /// - The color string will be preceded by '#'.
  /// - If the alpha channel is completely opaque, it will be ommitted.
  /// - Characters from 'a' to 'f' will be written in lowercase.
  fn to_hex(&self) -> String;
}

impl ColorUtils for Color32 {
  type Error = &'static str;

  fn lighten(&self, factor: f32) -> Self {
    Color32::from_rgba_premultiplied(
      (self.r() as f32 * factor) as u8,
      (self.g() as f32 * factor) as u8,
      (self.b() as f32 * factor) as u8,
      self.a(),
    )
  }

  fn from_hex(hex: &str) -> Result<Self, Self::Error> {
    // Convert a hex string to decimal. Eg. "00" -> 0. "FF" -> 255.
    fn _hex_dec(hex_string: &str) -> Result<u8, &'static str> {
      match u8::from_str_radix(hex_string, 16) {
        Ok(o) => Ok(o),
        Err(_) => Err("Error parsing hex"),
      }
    }

    if hex.len() == 9 && hex.starts_with('#') {
      // #FFFFFFFF (Red Green Blue Alpha)
      return Ok(Color32::from_rgba_premultiplied(
        _hex_dec(&hex[1..3])?,
        _hex_dec(&hex[3..5])?,
        _hex_dec(&hex[5..7])?,
        _hex_dec(&hex[7..9])?,
      ));
    } else if hex.len() == 7 && hex.starts_with('#') {
      // #FFFFFF (Red Green Blue)
      return Ok(Color32::from_rgb(
        _hex_dec(&hex[1..3])?,
        _hex_dec(&hex[3..5])?,
        _hex_dec(&hex[5..7])?,
      ));
    }

    Err("Error parsing hex. Example of valid formats: #FFFFFF or #ffffffff")
  }

  fn to_hex(&self) -> String {
    if self.a() < 255 {
      format!(
        "#{:02x?}{:02x?}{:02x?}{:02x?}",
        self.r(),
        self.g(),
        self.b(),
        self.a()
      )
    } else {
      format!("#{:02x?}{:02x?}{:02x?}", self.r(), self.g(), self.b())
    }
  }
}
