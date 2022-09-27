/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use self::syntax_highlighting::highlight;
use self::syntax_highlighting::CodeTheme;
use super::CodeEditor;
use crate::shard::Shard;
use crate::shards::gui::util;
use crate::shards::gui::EguiId;
use crate::shards::gui::ImmutableVar;
use crate::shards::gui::MutableVar;
use crate::shards::gui::EGUI_UI_SEQ_TYPE;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::shards::gui::STRING_VAR_SLICE;
use crate::shardsc;
use crate::types::common_type;
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::InstanceData;
use crate::types::OptionalString;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Type;
use crate::types::Types;
use crate::types::Var;

use crate::types::NONE_TYPES;
use crate::types::STRING_TYPES;
use std::cmp::Ordering;
use std::ffi::CStr;

mod syntax_highlighting;

lazy_static! {
  static ref CODEEDITOR_PARAMETERS: Parameters = vec![
    (
      cstr!("Variable"),
      cstr!("The variable that holds the input value."),
      STRING_VAR_SLICE,
    )
      .into(),
    (
      cstr!("Language"),
      cstr!("The name of the programming language for syntax highlighting."),
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

impl Shard for CodeEditor {
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
    OptionalString(shccstr!("A TextInput with support for highlighting"))
  }

  fn inputTypes(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("The value is ignored."))
  }

  fn outputTypes(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("The value produced when changed."))
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&CODEEDITOR_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.variable.set_param(value)),
      1 => Ok(self.language.set_param(value)),
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
            return Err("TextInput: string variable required.");
          }
          break;
        }
      }
    } else {
      self.mutable_text = false;
    }

    Ok(common_type::STRING)
  }

  fn exposedVariables(&mut self) -> Option<&ExposedTypes> {
    if self.variable.is_variable() && self.should_expose {
      self.exposing.clear();

      let exp_info = ExposedInfo {
        exposedType: common_type::STRING,
        name: self.variable.get_name(),
        help: cstr!("The exposed string variable").into(),
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

    // Add UI.Parents to the list of required variables
    let exp_info = ExposedInfo {
      exposedType: EGUI_UI_SEQ_TYPE,
      name: self.parents.get_name(),
      help: cstr!("The parent UI objects.").into(),
      ..ExposedInfo::default()
    };
    self.requiring.push(exp_info);

    Some(&self.requiring)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.parents.warmup(ctx);
    self.variable.warmup(ctx);
    self.language.warmup(ctx);

    if self.should_expose {
      self.variable.get_mut().valueType = common_type::STRING.basicType;
    }

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.language.cleanup();
    self.variable.cleanup();
    self.parents.cleanup();

    Ok(())
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
    if let Some(ui) = util::get_current_parent(*self.parents.get())? {
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
      let mut layouter = |ui: &egui::Ui, string: &str, wrap_width: f32| {
        let mut layout_job = highlight(ui.ctx(), &theme, string, language);
        layout_job.wrap.max_width = wrap_width;
        ui.fonts().layout_job(layout_job)
      };
      let id_source = EguiId::new(self, 0);
      let mut mutable;
      let mut immutable;
      let text: &mut dyn egui::TextBuffer = if self.mutable_text {
        mutable = MutableVar(self.variable.get_mut());
        &mut mutable
      } else {
        immutable = ImmutableVar(self.variable.get());
        &mut immutable
      };
      let code_editor = egui::TextEdit::multiline(text)
        .code_editor()
        .desired_width(f32::INFINITY)
        .layouter(&mut layouter);
      let response = egui::ScrollArea::vertical()
        .id_source(id_source)
        .show(ui, |ui| ui.centered_and_justified(|ui| ui.add(code_editor)))
        .inner
        .inner;

      if response.changed() || response.lost_focus() {
        Ok(*self.variable.get())
      } else {
        Ok(Var::default())
      }
    } else {
      Err("No UI parent")
    }
  }
}
