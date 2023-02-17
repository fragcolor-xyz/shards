/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::Plot;
use super::EGUI_PLOT_UI_TYPE;
use super::PLOT_UI_NAME;
use crate::shard::Shard;
use crate::shards::gui::util;
use crate::shards::gui::EguiId;
use crate::shards::gui::BOOL_OR_NONE_SLICE;
use crate::shards::gui::FLOAT_VAR_OR_NONE_SLICE;
use crate::shards::gui::HELP_OUTPUT_EQUAL_INPUT;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::InstanceData;
use crate::types::OptionalString;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Seq;
use crate::types::ShardsVar;
use crate::types::Type;
use crate::types::Types;
use crate::types::Var;
use crate::types::WireState;
use crate::types::ANY_TYPES;
use crate::types::SHARDS_OR_NONE_TYPES;

lazy_static! {
  static ref PLOT_PARAMETERS: Parameters = vec![
    (
      cstr!("Contents"),
      shccstr!("The UI contents."),
      &SHARDS_OR_NONE_TYPES[..],
    )
      .into(),
    (
      cstr!("ViewAspect"),
      shccstr!("Width / height ratio of the plot region."),
      FLOAT_VAR_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("DataAspect"),
      shccstr!("Width / height ratio of the data."),
      FLOAT_VAR_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("Legend"),
      shccstr!("Whether to display the legend."),
      BOOL_OR_NONE_SLICE,
    )
      .into(),
  ];
}

impl Default for Plot {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    let mut plot_context = ParamVar::default();
    plot_context.set_name(PLOT_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      contents: ShardsVar::default(),
      view_aspect: ParamVar::default(),
      data_aspect: ParamVar::default(),
      legend: ParamVar::default(),
      plot_context,
      exposing: Vec::new(),
    }
  }
}

impl Shard for Plot {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.Plot")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.Plot-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.Plot"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("A 2D plot area."))
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "The value that will be passed to the Contents shards of the plot."
    ))
  }

  fn outputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    *HELP_OUTPUT_EQUAL_INPUT
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&PLOT_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => self.contents.set_param(value),
      1 => Ok(self.view_aspect.set_param(value)),
      2 => Ok(self.data_aspect.set_param(value)),
      3 => Ok(self.legend.set_param(value)),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.contents.get_param(),
      1 => self.view_aspect.get_param(),
      2 => self.data_aspect.get_param(),
      3 => self.legend.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Parents to the list of required variables
    util::require_parents(&mut self.requiring, &self.parents);

    Some(&self.requiring)
  }

  fn exposedVariables(&mut self) -> Option<&ExposedTypes> {
    self.exposing.clear();

    if util::expose_contents_variables(&mut self.exposing, &self.contents) {
      Some(&self.exposing)
    } else {
      None
    }
  }

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    // we need to inject the UI context to the inner shards
    let mut data = *data;
    // clone shared into a new vector we can append things to
    let mut shared: ExposedTypes = data.shared.into();
    // expose UI context
    let ctx_info = ExposedInfo {
      exposedType: EGUI_PLOT_UI_TYPE,
      name: self.plot_context.get_name(),
      help: cstr!("The UI plot context.").into(),
      isMutable: false,
      isProtected: true, // don't allow to be used in code/wires
      isTableEntry: false,
      global: false,
      isPushTable: false,
    };
    shared.push(ctx_info);
    // update shared
    data.shared = (&shared).into();

    if !self.contents.is_empty() {
      self.contents.compose(&data)?;
    }

    Ok(data.inputType)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.parents.warmup(ctx);
    self.plot_context.warmup(ctx);
    self.plot_context.set_fast_unsafe(&Seq::new().as_ref().into());

    if !self.contents.is_empty() {
      self.contents.warmup(ctx)?;
    }
    self.view_aspect.warmup(ctx);
    self.data_aspect.warmup(ctx);
    self.legend.warmup(ctx);

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.legend.cleanup();
    self.data_aspect.cleanup();
    self.view_aspect.cleanup();
    if !self.contents.is_empty() {
      self.contents.cleanup();
    }

    self.plot_context.cleanup();
    self.parents.cleanup();

    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    if self.contents.is_empty() {
      return Ok(*input);
    }

    if let Some(ui) = util::get_current_parent(self.parents.get())? {
      let mut plot = egui::plot::Plot::new(EguiId::new(self, 0));

      let view_aspect = self.view_aspect.get();
      if !view_aspect.is_none() {
        plot = plot.view_aspect(view_aspect.try_into()?);
      }

      let data_aspect = self.data_aspect.get();
      if !data_aspect.is_none() {
        plot = plot.data_aspect(data_aspect.try_into()?);
      }

      let legend = self.legend.get();
      if !legend.is_none() {
        plot = plot.legend(Default::default());
      }

      plot
        .show(ui, |plot_ui| unsafe {
          let var = Var::new_object_from_ptr(plot_ui as *const _, &EGUI_PLOT_UI_TYPE);
          self.plot_context.set_cloning(&var);

          let mut _output = Var::default();
          if self.contents.activate(context, input, &mut _output) == WireState::Error {
            return Err("Failed to activate contents");
          }

          Ok(())
        })
        .inner?;

      Ok(*input)
    } else {
      Err("No UI parent")
    }
  }
}
