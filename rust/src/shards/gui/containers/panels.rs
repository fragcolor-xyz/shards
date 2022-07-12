/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::BottomPanel;
use super::CentralPanel;
use super::LeftPanel;
use super::RightPanel;
use super::TopPanel;
use crate::shard::Shard;
use crate::shards::gui::util;
use crate::shards::gui::EguiId;
use crate::shards::gui::CONTEXT_NAME;
use crate::shards::gui::EGUI_CTX_TYPE;
use crate::shards::gui::EGUI_UI_SEQ_TYPE;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::InstanceData;
use crate::types::OptionalString;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::RawString;
use crate::types::ShardsVar;
use crate::types::Type;
use crate::types::Types;
use crate::types::Var;
use crate::types::ANY_TYPES;
use crate::types::SHARDS_OR_NONE_TYPES;
use egui::Context as EguiNativeContext;

lazy_static! {
  static ref PANEL_PARAMETERS: Parameters = vec![(
    cstr!("Contents"),
    cstr!("The UI contents."),
    &SHARDS_OR_NONE_TYPES[..],
  )
    .into(),];
}

macro_rules! impl_panel {
  ($name:ident, $name_str:literal, $hash:literal, $egui_func:expr) => {
    impl Default for $name {
      fn default() -> Self {
        let mut ctx = ParamVar::default();
        ctx.set_name(CONTEXT_NAME);
        let mut parents = ParamVar::default();
        parents.set_name(PARENTS_UI_NAME);
        Self {
          instance: ctx,
          requiring: Vec::new(),
          contents: ShardsVar::default(),
          parents,
        }
      }
    }

    impl Shard for $name {
      fn registerName() -> &'static str {
        cstr!($name_str)
      }

      fn hash() -> u32 {
        compile_time_crc32::crc32!($hash)
      }

      fn name(&mut self) -> &str {
        $name_str
      }

      fn inputTypes(&mut self) -> &Types {
        &ANY_TYPES
      }

      fn outputTypes(&mut self) -> &Types {
        &ANY_TYPES
      }

      fn parameters(&mut self) -> Option<&Parameters> {
        Some(&PANEL_PARAMETERS)
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

        // Add UI.Context to the list of required variables
        let exp_info = ExposedInfo {
          exposedType: EGUI_CTX_TYPE,
          name: self.instance.get_name(),
          help: cstr!("The exposed UI context.").into(),
          ..ExposedInfo::default()
        };
        self.requiring.push(exp_info);

        Some(&self.requiring)
      }

      fn hasCompose() -> bool {
        true
      }

      fn compose(&mut self, data: &mut InstanceData) -> Result<Type, &str> {
        // we need to inject UI variable to the inner shards
        let mut data = *data;
        // clone shared into a new vector we can append things to
        let mut shared: ExposedTypes = data.shared.into();
        // append to shared ui vars
        let ui_info = ExposedInfo {
          exposedType: EGUI_UI_SEQ_TYPE,
          name: self.parents.get_name(),
          help: cstr!("The parent UI objects.").into(),
          isMutable: false,
          isProtected: true, // don't allow to be used in code/wires
          isTableEntry: false,
          global: false,
        };
        shared.push(ui_info);
        // update shared
        data.shared = (&shared).into();

        // center always last
        if !self.contents.is_empty() {
          self.contents.compose(&mut data)?;
        }

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

      fn cleanup(&mut self) -> Result<(), &str> {
        if !self.contents.is_empty() {
          self.contents.cleanup();
        }

        self.parents.cleanup();
        self.instance.cleanup();

        Ok(())
      }

      fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
        if !self.contents.is_empty() {
          let ui = util::get_current_parent(self.parents.get())?;
          let output = if let Some(ui) = ui {
            $egui_func(EguiId::new(self, 0)).show_inside(ui, |ui| {
              util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
            })
          } else {
            let gui_ctx = {
              let ctx_ptr: &mut EguiNativeContext =
                Var::from_object_ptr_mut_ref(self.instance.get(), &EGUI_CTX_TYPE)?;
              &*ctx_ptr
            };
            $egui_func(EguiId::new(self, 0)).show(gui_ctx, |ui| {
              util::activate_ui_contents(
                context,
                input,
                ui,
                &mut self.parents,
                &mut self.contents,
              )?;
              // when used as a top container, the input passes through to be consistent with Window
              Ok(*input)
            })
          }
          .inner?;

          return Ok(output);
        }

        Ok(*input)
      }
    }
  };
}

impl_panel!(
  BottomPanel,
  "UI.BottomPanel",
  "UI.BottomPanel-rust-0x20200101",
  egui::TopBottomPanel::bottom
);
impl_panel!(
  LeftPanel,
  "UI.LeftPanel",
  "UI.LeftPanel-rust-0x20200101",
  egui::SidePanel::left
);
impl_panel!(
  RightPanel,
  "UI.RightPanel",
  "UI.RightPanel-rust-0x20200101",
  egui::SidePanel::right
);
impl_panel!(
  TopPanel,
  "UI.TopPanel",
  "UI.TopPanel-rust-0x20200101",
  egui::TopBottomPanel::top
);

impl Default for CentralPanel {
  fn default() -> Self {
    let mut ctx = ParamVar::default();
    ctx.set_name(CONTEXT_NAME);
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      instance: ctx,
      requiring: Vec::new(),
      contents: ShardsVar::default(),
      parents,
    }
  }
}

impl Shard for CentralPanel {
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

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &ANY_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&PANEL_PARAMETERS)
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

    // Add UI.Context to the list of required variables
    let exp_info = ExposedInfo {
      exposedType: EGUI_CTX_TYPE,
      name: self.instance.get_name(),
      help: cstr!("The exposed UI context.").into(),
      ..ExposedInfo::default()
    };
    self.requiring.push(exp_info);

    Some(&self.requiring)
  }

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, data: &mut InstanceData) -> Result<Type, &str> {
    // we need to inject UI variable to the inner shards
    let mut data = *data;
    // clone shared into a new vector we can append things to
    let mut shared: ExposedTypes = data.shared.into();
    // append to shared ui vars
    let ui_info = ExposedInfo {
      exposedType: EGUI_UI_SEQ_TYPE,
      name: self.parents.get_name(),
      help: cstr!("The parent UI objects.").into(),
      isMutable: false,
      isProtected: true, // don't allow to be used in code/wires
      isTableEntry: false,
      global: false,
    };
    shared.push(ui_info);
    // update shared
    data.shared = (&shared).into();

    // center always last
    if !self.contents.is_empty() {
      self.contents.compose(&mut data)?;
    }

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

  fn cleanup(&mut self) -> Result<(), &str> {
    if !self.contents.is_empty() {
      self.contents.cleanup();
    }

    self.parents.cleanup();
    self.instance.cleanup();

    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    let gui_ctx = {
      let ctx_ptr: &mut EguiNativeContext =
        Var::from_object_ptr_mut_ref(self.instance.get(), &EGUI_CTX_TYPE)?;
      &*ctx_ptr
    };

    if !self.contents.is_empty() {
      let ui = util::get_current_parent(self.parents.get())?;
      let output = if let Some(ui) = ui {
        egui::CentralPanel::default().show_inside(ui, |ui| {
          util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
        })
      } else {
        egui::CentralPanel::default().show(gui_ctx, |ui| {
          util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)?;
          // when used as a top container, the input passes through to be consistent with Window
          Ok(*input)
        })
      }
      .inner?;

      return Ok(output);
    }

    Ok(*input)
  }
}
