/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::MarkerShape;
use super::PlotPoints;
use super::EGUI_PLOT_UI_TYPE;
use super::PLOT_UI_NAME;
use super::SEQ_OF_FLOAT2_TYPES;
use crate::widgets::plots::MARKER_SHAPE_TYPES;
use crate::COLOR_VAR_OR_NONE_SLICE;
use crate::HELP_OUTPUT_EQUAL_INPUT;
use shards::shard::LegacyShard;
use shards::shardsc::SHColor;
use shards::types::Context;
use shards::types::ExposedInfo;
use shards::types::ExposedTypes;
use shards::types::OptionalString;
use shards::types::ParamVar;
use shards::types::Parameters;
use shards::types::Seq;
use shards::types::Types;
use shards::types::Var;
use shards::types::FLOAT_TYPES_SLICE;
use shards::types::STRING_OR_NONE_SLICE;

lazy_static! {
  static ref POINTS_PARAMETERS: Parameters = vec![
    (
      cstr!("Name"),
      shccstr!("Name of this chart, displayed in the plot legend."),
      STRING_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("Color"),
      shccstr!("Stroke color."),
      COLOR_VAR_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("Shape"),
      shccstr!("Shape of the marker."),
      &MARKER_SHAPE_TYPES[..],
    )
      .into(),
    (
      cstr!("Radius"),
      shccstr!("Radius of the marker."),
      FLOAT_TYPES_SLICE,
    )
      .into(),
  ];
}

impl Default for PlotPoints {
  fn default() -> Self {
    let mut plot_ui = ParamVar::default();
    plot_ui.set_name(PLOT_UI_NAME);
    Self {
      plot_ui,
      requiring: Vec::new(),
      name: ParamVar::default(),
      color: ParamVar::default(),
      shape: ParamVar::default(),
      radius: ParamVar::default(),
    }
  }
}

impl LegacyShard for PlotPoints {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.PlotPoints")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.PlotPoints-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.PlotPoints"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("Scattered points on a plot."))
  }

  fn inputTypes(&mut self) -> &Types {
    &SEQ_OF_FLOAT2_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("A sequence of point coordinates."))
  }

  fn outputTypes(&mut self) -> &Types {
    &SEQ_OF_FLOAT2_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    *HELP_OUTPUT_EQUAL_INPUT
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&POINTS_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => self.name.set_param(value),
      1 => self.color.set_param(value),
      2 => self.shape.set_param(value),
      3 => self.radius.set_param(value),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.name.get_param(),
      1 => self.color.get_param(),
      2 => self.shape.get_param(),
      3 => self.radius.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.PlotContext to the list of required variables
    let exp_info = ExposedInfo {
      exposedType: EGUI_PLOT_UI_TYPE,
      name: self.plot_ui.get_name(),
      help: shccstr!("The exposed UI plot context."),
      ..ExposedInfo::default()
    };
    self.requiring.push(exp_info);

    Some(&self.requiring)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.plot_ui.warmup(ctx);

    self.name.warmup(ctx);
    self.color.warmup(ctx);
    self.shape.warmup(ctx);
    self.radius.warmup(ctx);

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.radius.cleanup(ctx);
    self.shape.cleanup(ctx);
    self.color.cleanup(ctx);
    self.name.cleanup(ctx);

    self.plot_ui.cleanup(ctx);

    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    let plot_ui: &mut egui_plot::PlotUi =
      Var::from_object_ptr_mut_ref(self.plot_ui.get(), &EGUI_PLOT_UI_TYPE)?;

    let seq: Seq = input.try_into()?;
    let points = seq.iter().map(|x| {
      let v: (f64, f64) = x.as_ref().try_into().unwrap();
      [v.0, v.1]
    });
    let mut chart = egui_plot::Points::new(egui_plot::PlotPoints::from_iter(points));

    let name = self.name.get();
    if !name.is_none() {
      let name: &str = name.try_into()?;
      chart = chart.name(name);
    }

    let color = self.color.get();
    if !color.is_none() {
      let color: SHColor = color.try_into()?;
      chart = chart.color(egui::Color32::from_rgba_unmultiplied(
        color.r, color.g, color.b, color.a,
      ));
    }

    let shape = self.shape.get();
    if !shape.is_none() {
      chart = chart.shape(
        MarkerShape {
          bits: unsafe { shape.payload.__bindgen_anon_1.__bindgen_anon_3.enumValue },
        }
        .into(),
      );
    }

    let radius = self.radius.get();
    if !radius.is_none() {
      let radius: f32 = radius.try_into()?;
      chart = chart.radius(radius);
    }

    plot_ui.points(chart);

    Ok(*input)
  }
}

impl From<MarkerShape> for egui_plot::MarkerShape {
  fn from(value: MarkerShape) -> Self {
    match value {
      MarkerShape::Circle => egui_plot::MarkerShape::Circle,
      MarkerShape::Diamond => egui_plot::MarkerShape::Diamond,
      MarkerShape::Square => egui_plot::MarkerShape::Square,
      MarkerShape::Cross => egui_plot::MarkerShape::Cross,
      MarkerShape::Plus => egui_plot::MarkerShape::Plus,
      MarkerShape::Up => egui_plot::MarkerShape::Up,
      MarkerShape::Down => egui_plot::MarkerShape::Down,
      MarkerShape::Left => egui_plot::MarkerShape::Left,
      MarkerShape::Right => egui_plot::MarkerShape::Right,
      MarkerShape::Asterisk => egui_plot::MarkerShape::Asterisk,
      // default
      _ => egui_plot::MarkerShape::Circle,
    }
  }
}
