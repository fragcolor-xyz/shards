/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::BottomPanel;
use super::CentralPanel;
use super::LeftPanel;
use super::RightPanel;
use super::TopPanel;
use crate::util;
use crate::util::with_possible_panic;
use crate::EguiId;
use crate::CONTEXTS_NAME;
use crate::EGUI_CTX_TYPE;
use crate::HELP_OUTPUT_EQUAL_INPUT;
use crate::PARENTS_UI_NAME;
use shards::shard::LegacyShard;
use shards::types::Context;
use shards::types::ExposedInfo;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::OptionalString;
use shards::types::ParamVar;
use shards::types::Parameters;

use shards::types::ShardsVar;
use shards::types::Type;
use shards::types::Types;
use shards::types::Var;
use shards::types::ANY_TYPES;
use shards::types::BOOL_TYPES;
use shards::types::FLOAT_OR_NONE_TYPES_SLICE;
use shards::types::SHARDS_OR_NONE_TYPES;

lazy_static! {
  static ref CENTRALPANEL_PARAMETERS: Parameters = vec![(
    cstr!("Contents"),
    shccstr!("The UI contents."),
    &SHARDS_OR_NONE_TYPES[..],
  )
    .into(),];
  static ref SIDEPANEL_PARAMETERS: Parameters = vec![
    (
      cstr!("Resizable"),
      shccstr!("Whether the panel can be resized."),
      &BOOL_TYPES[..],
    )
      .into(),
    (
      cstr!("DefaultSize"),
      shccstr!("The initial size of the panel."),
      FLOAT_OR_NONE_TYPES_SLICE,
    )
      .into(),
    (
      cstr!("MinSize"),
      shccstr!("The minimum allowable size of the panel."),
      FLOAT_OR_NONE_TYPES_SLICE,
    )
      .into(),
    (
      cstr!("MaxSize"),
      shccstr!("The maximum allowable size of the panel."),
      FLOAT_OR_NONE_TYPES_SLICE,
    )
      .into(),
    (
      cstr!("Contents"),
      shccstr!("The UI contents."),
      &SHARDS_OR_NONE_TYPES[..],
    )
      .into(),
  ];
}

macro_rules! impl_panel {
  ($name:ident, $default_size:ident, $min_size:ident, $max_size:ident, $name_str:literal, $hash:literal, $egui_func:expr) => {
    impl Default for $name {
      fn default() -> Self {
        let mut ctx = ParamVar::default();
        ctx.set_name(CONTEXTS_NAME);
        let mut parents = ParamVar::default();
        parents.set_name(PARENTS_UI_NAME);
        Self {
          instance: ctx,
          requiring: Vec::new(),
          resizable: ParamVar::new(true.into()),
          $default_size: ParamVar::default(),
          $min_size: ParamVar::default(),
          $max_size: ParamVar::default(),
          contents: ShardsVar::default(),
          parents,
          exposing: Vec::new(),
        }
      }
    }

    impl LegacyShard for $name {
      fn registerName() -> &'static str {
        cstr!($name_str)
      }

      fn hash() -> u32 {
        compile_time_crc32::crc32!($hash)
      }

      fn name(&mut self) -> &str {
        $name_str
      }

      fn help(&mut self) -> OptionalString {
        OptionalString(shccstr!("Layout UI elements into the panel."))
      }

      fn inputTypes(&mut self) -> &Types {
        &ANY_TYPES
      }

      fn inputHelp(&mut self) -> OptionalString {
        OptionalString(shccstr!(
          "The value that will be passed to the Contents shards of the panel."
        ))
      }

      fn outputTypes(&mut self) -> &Types {
        &ANY_TYPES
      }

      fn outputHelp(&mut self) -> OptionalString {
        *HELP_OUTPUT_EQUAL_INPUT
      }

      fn parameters(&mut self) -> Option<&Parameters> {
        Some(&SIDEPANEL_PARAMETERS)
      }

      fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
        match index {
          0 => self.resizable.set_param(value),
          1 => self.$default_size.set_param(value),
          2 => self.$min_size.set_param(value),
          3 => self.$max_size.set_param(value),
          4 => self.contents.set_param(value),
          _ => Err("Invalid parameter index"),
        }
      }

      fn getParam(&mut self, index: i32) -> Var {
        match index {
          0 => self.resizable.get_param(),
          1 => self.$default_size.get_param(),
          2 => self.$min_size.get_param(),
          3 => self.$max_size.get_param(),
          4 => self.contents.get_param(),
          _ => Var::default(),
        }
      }

      fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
        self.requiring.clear();

        util::require_context(&mut self.requiring);
        util::require_parents(&mut self.requiring);

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
        if !self.contents.is_empty() {
          self.contents.compose(&data)?;
        }

        // Always passthrough the input
        Ok(data.inputType)
      }

      fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
        self.instance.warmup(ctx);
        self.parents.warmup(ctx);

        self.resizable.warmup(ctx);
        self.$default_size.warmup(ctx);
        self.$min_size.warmup(ctx);
        self.$max_size.warmup(ctx);
        if !self.contents.is_empty() {
          self.contents.warmup(ctx)?;
        }

        Ok(())
      }

      fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
        if !self.contents.is_empty() {
          self.contents.cleanup(ctx);
        }
        self.$max_size.cleanup(ctx);
        self.$min_size.cleanup(ctx);
        self.$default_size.cleanup(ctx);
        self.resizable.cleanup(ctx);

        self.parents.cleanup(ctx);
        self.instance.cleanup(ctx);

        Ok(())
      }

      fn activate(&mut self, context: &Context, input: &Var) -> Result<Option<Var>, &str> {
        if self.contents.is_empty() {
          return Ok(None);
        }

        let mut panel = $egui_func(EguiId::new(self, 0));

        let resizable = self.resizable.get();
        panel = panel.resizable(resizable.try_into()?);

        let $default_size = self.$default_size.get();
        if !$default_size.is_none() {
          panel = panel.$default_size($default_size.try_into()?);
        }
        let $min_size = self.$min_size.get();
        if !$min_size.is_none() {
          panel = panel.$min_size($min_size.try_into()?);
        }
        let $max_size = self.$max_size.get();
        if !$max_size.is_none() {
          panel = panel.$max_size($max_size.try_into()?);
        }

        with_possible_panic(|| {
          if let Some(ui) = util::get_current_parent_opt(self.parents.get())? {
            panel
              .show_inside(ui, |ui| {
                util::activate_ui_contents(
                  context,
                  input,
                  ui,
                  &mut self.parents,
                  &mut self.contents,
                )
              })
              .inner
          } else {
            let egui_ctx = &util::get_current_context(&self.instance)?.egui_ctx;
            panel
              .show(egui_ctx, |ui| {
                util::activate_ui_contents(
                  context,
                  input,
                  ui,
                  &mut self.parents,
                  &mut self.contents,
                )
              })
              .inner
          }
        })??;

        // Always passthrough the input
        Ok(None)
      }
    }
  };
}

impl_panel!(
  BottomPanel,
  default_height,
  min_height,
  max_height,
  "UI.BottomPanel",
  "UI.BottomPanel-rust-0x20200101",
  egui::TopBottomPanel::bottom
);
impl_panel!(
  LeftPanel,
  default_width,
  min_width,
  max_width,
  "UI.LeftPanel",
  "UI.LeftPanel-rust-0x20200101",
  egui::SidePanel::left
);
impl_panel!(
  RightPanel,
  default_width,
  min_width,
  max_width,
  "UI.RightPanel",
  "UI.RightPanel-rust-0x20200101",
  egui::SidePanel::right
);
impl_panel!(
  TopPanel,
  default_height,
  min_height,
  max_height,
  "UI.TopPanel",
  "UI.TopPanel-rust-0x20200101",
  egui::TopBottomPanel::top
);

impl Default for CentralPanel {
  fn default() -> Self {
    let mut ctx = ParamVar::default();
    ctx.set_name(CONTEXTS_NAME);
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      instance: ctx,
      requiring: Vec::new(),
      contents: ShardsVar::default(),
      parents,
      exposing: Vec::new(),
    }
  }
}

impl LegacyShard for CentralPanel {
  fn registerName() -> &'static str {
    cstr!("UI.CentralPanel")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("UI.CentralPanel-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.CentralPanel"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("Layout UI elements into the central panel."))
  }

  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &ANY_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "The value that will be passed to the Contents shards of the panel."
    ))
  }

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &ANY_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    *HELP_OUTPUT_EQUAL_INPUT
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&CENTRALPANEL_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => self.contents.set_param(value),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.contents.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Contexts to the list of required variables
    let exp_info = ExposedInfo {
      exposedType: EGUI_CTX_TYPE,
      name: self.instance.get_name(),
      help: shccstr!("The exposed UI context."),
      ..ExposedInfo::default()
    };
    self.requiring.push(exp_info);

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
    if !self.contents.is_empty() {
      self.contents.compose(&data)?;
    }

    // Always passthrough the input
    Ok(data.inputType)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.instance.warmup(ctx);
    self.parents.warmup(ctx);

    if !self.contents.is_empty() {
      self.contents.warmup(ctx)?;
    }

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    if !self.contents.is_empty() {
      self.contents.cleanup(ctx);
    }

    self.parents.cleanup(ctx);
    self.instance.cleanup(ctx);

    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    if self.contents.is_empty() {
      return Ok(None);
    }

    let ui_ctx = util::get_current_context(&self.instance)?;

    with_possible_panic(|| {
      if let Some(ui) = util::get_current_parent_opt(self.parents.get())? {
        egui::CentralPanel::default()
          .show_inside(ui, |ui| {
            util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
          })
          .inner
      } else {
        egui::CentralPanel::default()
          .show(&ui_ctx.egui_ctx, |ui| {
            util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
          })
          .inner
      }
    })??;

    // Always passthrough the input
    Ok(None)
  }
}
