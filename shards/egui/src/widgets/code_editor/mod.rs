/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use self::syntax_highlighting::CodeTheme;
use super::CodeEditor;
use crate::util;
use crate::EguiId;
use crate::MutVarTextBuffer;
use crate::VarTextBuffer;
use crate::HELP_VALUE_IGNORED;
use crate::PARENTS_UI_NAME;
use crate::STRING_VAR_SLICE;
use shards::shard::LegacyShard;
use shards::shardsc;
use shards::types::common_type;
use shards::types::Context;
use shards::types::ExposedInfo;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::OptionalString;
use shards::types::ParamVar;
use shards::types::Parameters;
use shards::types::Type;
use shards::types::Types;
use shards::types::Var;
use shards::types::BOOL_TYPES;

use shards::types::NONE_TYPES;
use syntax_highlighting::highlight_generic;
use syntax_highlighting::highlight_shards;

use std::cmp::Ordering;
use std::ffi::CStr;

mod syntax_highlighting;

lazy_static! {
  static ref CODEEDITOR_PARAMETERS: Parameters = vec![
    (
      cstr!("Code"),
      shccstr!("The variable that holds the code to edit."),
      STRING_VAR_SLICE,
    )
      .into(),
    (
      cstr!("Language"),
      shccstr!("The name of the programming language for syntax highlighting."),
      STRING_VAR_SLICE,
    )
      .into(),
  ];
}

impl Default for CodeEditor {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      variable: ParamVar::default(),
      language: ParamVar::default(),
      exposing: Vec::new(),
      should_expose: false,
      mutable_text: true,
    }
  }
}

impl LegacyShard for CodeEditor {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.CodeEditor")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.CodeEditor-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.CodeEditor"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("A TextField with support for highlighting."))
  }

  fn inputTypes(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    *HELP_VALUE_IGNORED
  }

  fn outputTypes(&mut self) -> &Types {
    &BOOL_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("The value produced when changed."))
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&CODEEDITOR_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => self.variable.set_param(value),
      1 => self.language.set_param(value),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.variable.get_param(),
      1 => self.language.get_param(),
      _ => Var::default(),
    }
  }

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    if self.variable.is_variable() {
      self.should_expose = true; // assume we expose a new variable

      let shared: ExposedTypes = data.shared.into();
      for var in shared {
        let (a, b) = unsafe {
          (
            CStr::from_ptr(var.name),
            CStr::from_ptr(self.variable.get_name()),
          )
        };
        if CStr::cmp(&a, &b) == Ordering::Equal {
          self.should_expose = false;
          self.mutable_text = var.isMutable;
          if var.exposedType.basicType != shardsc::SHType_String {
            return Err("TextField: string variable required.");
          }
          break;
        }
      }
    } else {
      self.mutable_text = false;
    }

    Ok(common_type::bool)
  }

  fn exposedVariables(&mut self) -> Option<&ExposedTypes> {
    if self.variable.is_variable() && self.should_expose {
      self.exposing.clear();

      let exp_info = ExposedInfo {
        exposedType: common_type::string,
        name: self.variable.get_name(),
        help: shccstr!("The exposed string variable"),
        ..ExposedInfo::default()
      };

      self.exposing.push(exp_info);
      Some(&self.exposing)
    } else {
      None
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();
    util::require_parents(&mut self.requiring);
    Some(&self.requiring)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.parents.warmup(ctx);
    self.variable.warmup(ctx);
    self.language.warmup(ctx);

    if self.should_expose {
      self.variable.get_mut().valueType = common_type::string.basicType;
    }

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.language.cleanup(ctx);
    self.variable.cleanup(ctx);
    self.parents.cleanup(ctx);

    Ok(())
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
    if let Some(ui) = util::get_current_parent_opt(self.parents.get())? {
      let theme = if ui.style().visuals.dark_mode {
        CodeTheme::dark()
      } else {
        CodeTheme::light()
      };
      let language = self.language.get();
      let language = if language.is_none() {
        ""
      } else {
        language.try_into()?
      };

      let id1 = EguiId::new(self, 0);
      let id2 = EguiId::new(self, 1);

      let mut layouter = |ui: &egui::Ui, string: &str, wrap_width: f32| {
        let mut layout_job = if language == "shards" {
          highlight_shards(ui.ctx(), &theme, string)
        } else {
          highlight_generic(ui.ctx(), &theme, string, language)
        };
        layout_job.wrap.max_width = wrap_width;
        ui.fonts(|f| f.layout_job(layout_job))
      };
      let mut mutable;
      let mut immutable;
      let text: &mut dyn egui::TextBuffer = if self.mutable_text {
        mutable = MutVarTextBuffer(self.variable.get_mut());
        &mut mutable
      } else {
        immutable = VarTextBuffer(self.variable.get());
        &mut immutable
      };

      let response = ui
        .scope(|ui| {
          let style = ui.style_mut();

          if let Some(bg_color) = theme.theme.settings.background {
            style.visuals.extreme_bg_color = syntect_color_to_egui(bg_color);
          }

          if let Some(selection_foreground) = theme.theme.settings.selection_foreground {
            style.visuals.selection.bg_fill = syntect_color_to_egui(selection_foreground);
          }

          let code_editor = egui::TextEdit::multiline(text)
            .id_source(id1)
            .code_editor()
            .desired_width(f32::INFINITY)
            .layouter(&mut layouter);
          egui::ScrollArea::vertical()
            .id_source(id2)
            .show(ui, |ui| ui.centered_and_justified(|ui| ui.add(code_editor)))
            .inner
            .inner
        })
        .inner;

      if response.changed() || response.lost_focus() {
        Ok(true.into())
      } else {
        Ok(false.into())
      }
    } else {
      Err("No UI parent")
    }
  }
}

fn syntect_color_to_egui(color: syntect::highlighting::Color) -> egui::Color32 {
  egui::Color32::from_rgb(color.r, color.g, color.b)
}
