/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::PlotBar;
use super::EGUI_PLOT_UI_TYPE;
use super::PLOT_UI_NAME;
use super::SEQ_OF_FLOAT2_TYPES;
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
use shards::types::BOOL_VAR_OR_NONE_SLICE;
use shards::types::FLOAT_TYPES;
use shards::types::STRING_OR_NONE_SLICE;

lazy_static! {
  static ref BAR_PARAMETERS: Parameters = vec![
    (
      cstr!("Color"),
      shccstr!("Stroke color."),
      COLOR_VAR_OR_NONE_SLICE,
    )
      .into(),
    (cstr!("Width"), cstr!("Width of a bar."), &FLOAT_TYPES[..],).into(),
    (
      cstr!("Horizontal"),
      shccstr!("Display the bars horizontally."),
      BOOL_VAR_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("Name"),
      shccstr!("Name of this chart, displayed in the plot legend."),
      STRING_OR_NONE_SLICE,
    )
      .into(),
  ];
}

impl Default for PlotBar {
  fn default() -> Self {
    let mut plot_ui = ParamVar::default();
    plot_ui.set_name(PLOT_UI_NAME);
    Self {
      plot_ui,
      requiring: Vec::new(),
      color: ParamVar::default(),
      bar_width: ParamVar::default(),
      horizontal: ParamVar::default(),
      name: ParamVar::default(),
    }
  }
}

impl LegacyShard for PlotBar {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.PlotBar")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.PlotBar-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.PlotBar"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "Points represented as vertical or horizontal bars on a plot."
    ))
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
    Some(&BAR_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => self.color.set_param(value),
      1 => self.bar_width.set_param(value),
      2 => self.horizontal.set_param(value),
      3 => self.name.set_param(value),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.color.get_param(),
      1 => self.bar_width.get_param(),
      2 => self.horizontal.get_param(),
      3 => self.name.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.PlotContext to the list of required variables
    let exp_info = ExposedInfo {
      exposedType: EGUI_PLOT_UI_TYPE,
      name: self.plot_ui.get_name(),
      help: shccstr!("Plot context."),
      ..ExposedInfo::default()
    };
    self.requiring.push(exp_info);

    Some(&self.requiring)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.plot_ui.warmup(ctx);
    self.color.warmup(ctx);
    self.bar_width.warmup(ctx);
    self.horizontal.warmup(ctx);
    self.name.warmup(ctx);

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.name.cleanup(ctx);
    self.horizontal.cleanup(ctx);
    self.bar_width.cleanup(ctx);
    self.color.cleanup(ctx);
    self.plot_ui.cleanup(ctx);

    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let plot_ui: &mut egui_plot::PlotUi =
      Var::from_object_ptr_mut_ref(self.plot_ui.get(), &EGUI_PLOT_UI_TYPE)?;

    let seq: Seq = input.try_into()?;
    let points = seq.iter().map(|x| {
      let v: (f64, f64) = x.as_ref().try_into().unwrap();
      v
    });

    let bar_width = self.bar_width.get();
    let bar_width = if !bar_width.is_none() {
      let bar_width: f64 = bar_width.try_into()?;
      Some(bar_width)
    } else {
      None
    };

    let mut chart = egui_plot::BarChart::new(
      points
        .map(|(x, y)| {
          let mut bar = egui_plot::Bar::new(x, y);
          if let Some(bar_width) = bar_width {
            bar = bar.width(bar_width);
          }
          bar
        })
        .collect(),
    );

    let color = self.color.get();
    if !color.is_none() {
      let color: SHColor = color.try_into()?;
      chart = chart.color(egui::Color32::from_rgba_unmultiplied(
        color.r, color.g, color.b, color.a,
      ));
    }

    let horizontal = self.horizontal.get();
    if !horizontal.is_none() && horizontal.try_into()? {
      chart = chart.horizontal();
    }

    let name = self.name.get();
    if !name.is_none() {
      let name: &str = name.try_into()?;
      chart = chart.name(name);
    }

    plot_ui.bar_chart(chart);

    Ok(None)
  }
}
