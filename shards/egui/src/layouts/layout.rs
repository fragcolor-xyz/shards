/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::Layout;
use crate::util;
use crate::HELP_OUTPUT_EQUAL_INPUT;
use crate::PARENTS_UI_NAME;
use shards::shard::Shard;
use shards::types::common_type;
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
use shards::types::SHARDS_OR_NONE_TYPES;
use shards::types::STRING_OR_NONE_SLICE;
use shards::types::BOOL_VAR_OR_NONE_SLICE;
use crate::FLOAT2_VAR_SLICE;

lazy_static! {
  static ref LAYOUT_PARAMETERS: Parameters = vec![
    (
      cstr!("Contents"),
      shccstr!("The UI contents."),
      &SHARDS_OR_NONE_TYPES[..],
    )
      .into(),
    (
      cstr!("Direction"),
      shccstr!("The main axis direction. LeftToRight, RightToLeft, TopDown, or BottomUp."),
      STRING_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("MainWrap"),
      shccstr!("If true, wrap around when reaching the end of the main direction."),
      BOOL_VAR_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("MainAlign"),
      shccstr!("The alignment of content on the main axis. Min, Center or Max."),
      STRING_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("MainJustify"),
      shccstr!("Whether to justify the main axis."),
      BOOL_VAR_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("CrossAlign"),
      shccstr!("The alignment of content on the cross axis. Min, Center or Max."),
      STRING_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("CrossJustify"),
      shccstr!("Whether to justify the cross axis. Whether widgets get maximum width/height for vertical/horizontal layouts."),
      BOOL_VAR_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("DesiredSize"),
      shccstr!("Whether to justify the cross axis. Whether widgets get maximum width/height for vertical/horizontal layouts."),
      FLOAT2_VAR_SLICE,
    )
      .into(),
  ];
}

impl Default for Layout {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      contents: ShardsVar::default(),
      main_dir: ParamVar::default(),
      main_wrap: ParamVar::default(),
      main_align: ParamVar::default(),
      main_justify: ParamVar::default(),
      cross_align: ParamVar::default(),
      cross_justify: ParamVar::default(),
      desired_size: ParamVar::default(), 
      exposing: Vec::new(),
    }
  }
}

impl Shard for Layout {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.Layout")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.Layout-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.Layout"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("Versatile layout with many options for customizing the desired UI."))
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "The value that will be passed to the Contents shards of the layout."
    ))
  }

  fn outputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    *HELP_OUTPUT_EQUAL_INPUT
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&LAYOUT_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => self.contents.set_param(value),
      1 => Ok(self.main_dir.set_param(value)),
      2 => Ok(self.main_wrap.set_param(value)),
      3 => Ok(self.main_align.set_param(value)),
      4 => Ok(self.main_justify.set_param(value)),
      5 => Ok(self.cross_align.set_param(value)),
      6 => Ok(self.cross_justify.set_param(value)),
      7 => Ok(self.desired_size.set_param(value)),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.contents.get_param(),
      1 => self.main_dir.get_param(),
      2 => self.main_wrap.get_param(),
      3 => self.main_align.get_param(),
      4 => self.main_justify.get_param(),
      5 => self.cross_align.get_param(),
      6 => self.cross_justify.get_param(),
      7 => self.desired_size.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Parents to the list of required variables
    util::require_parents(&mut self.requiring, &self.parents);

    // Add main axis direction to the list of required variables
    // TODO: Check if this works
    let main_dir_info = ExposedInfo {
      exposedType: common_type::string,
      name: self.main_dir.get_name(),
      help: cstr!("The main axis direction.").into(),
      ..ExposedInfo::default()
    };
    self.requiring.push(main_dir_info);

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
      self.contents.compose(data)?;
    }

    // Always passthrough the input
    Ok(data.inputType)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.parents.warmup(ctx);
    if !self.contents.is_empty() {
      self.contents.warmup(ctx)?;
    }
    self.main_dir.warmup(ctx);
    self.main_wrap.warmup(ctx);
    self.main_align.warmup(ctx);
    self.main_justify.warmup(ctx);
    self.cross_align.warmup(ctx);
    self.cross_justify.warmup(ctx);
    self.desired_size.warmup(ctx);

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.desired_size.cleanup();
    self.cross_justify.cleanup();
    self.cross_align.cleanup();
    self.main_justify.cleanup();
    self.main_align.cleanup();
    self.main_wrap.cleanup();
    self.main_dir.cleanup();
    if !self.contents.is_empty() {
      self.contents.cleanup();
    }
    self.parents.cleanup();

    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    if self.contents.is_empty() {
      return Ok(*input);
    }

    if let Some(ui) = util::get_current_parent(self.parents.get())? {

      // let main_dir: &str = self.main_dir.get().try_into()?;
      // let main_wrap = self.main_wrap.get();
      // let main_align = self.main_align.get();
      // let main_justify = self.main_justify.get();
      // let cross_align = self.cross_align.get();
      // let cross_justify = self.cross_justify.get();
      // let desired_size = self.desired_size.get();

      // let layout = egui::Layout::default();

      // let layout = if let Ok(main_dir) = self.main_dir.get().try_into()? {
      //   match main_dir {
      //     "LeftToRight" => Layout::horizontal(self.main_wrap.get()),
      //     "RightToLeft" => Layout::horizontal_reverse(self.main_wrap.get()),
      //     "TopDown" => Layout::vertical(self.main_wrap.get()),
      //     "BottomUp" => Layout::vertical_reverse(self.main_wrap.get()),
      //   }
      // } else {
      //   Layout::default()
      // };

      // if !desired_size.is_none() {
      //   ui.allocate_ui_with_layout(desired_size.try_into()?, )
      // }

      // if self.centered {
      //   ui.vertical_centered(|ui| {
      //     util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
      //   })
      //   .inner?
      // } else {
      //   ui.vertical(|ui| {
      //     util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
      //   })
      //   .inner?
      // };

      // Always passthrough the input
      Ok(*input)
    } else {
      Err("No UI parent")
    }
  }
}
