use super::util;
use super::CONTEXTS_NAME;
use super::EGUI_CTX_TYPE;
use super::PARENTS_UI_NAME;
use crate::bindings::egui_FullOutput;
use crate::bindings::egui_Input;
use crate::bindings::make_native_full_output;
use crate::bindings::NativeFullOutput;
use crate::Context as UIContext;
use crate::EGUI_UI_TYPE;
use shards::core::Core;
use shards::shardsc::Shards;
use shards::types::ClonedVar;
use shards::types::Context as ShardsContext;
use shards::types::ExposedInfo;
use shards::types::ParamVar;
use shards::types::Var;
use shards::types::WireState;
use std::cell::RefCell;

pub struct EguiHost {
  context: Option<UIContext>,
  full_output: Option<NativeFullOutput>,
  instance: ParamVar,
  parents: ParamVar,
  exposed: Vec<ExposedInfo>,
}

impl Default for EguiHost {
  fn default() -> Self {
    let mut instance = ParamVar::new_named(CONTEXTS_NAME);
    let mut parents = ParamVar::new_named(PARENTS_UI_NAME);

    let exposed = vec![
      ExposedInfo {
        exposedType: EGUI_CTX_TYPE,
        name: instance.get_name(),
        help: cstr!("The UI context.").into(),
        isMutable: false,
        isProtected: true, // don't allow to be used in code/wires
        isTableEntry: false,
        global: false,
        isPushTable: false,
        exposed: false,
      },
      ExposedInfo {
        exposedType: EGUI_UI_TYPE,
        name: parents.get_name(),
        help: cstr!("The parent UI objects.").into(),
        isMutable: false,
        isProtected: true, // don't allow to be used in code/wires
        isTableEntry: false,
        global: false,
        isPushTable: false,
        exposed: false,
      },
    ];

    Self {
      context: None,
      full_output: None,
      instance: instance,
      parents: parents,
      exposed: exposed,
    }
  }
}

impl EguiHost {
  pub fn get_exposed_info(&self) -> &Vec<ExposedInfo> {
    &self.exposed
  }

  pub fn warmup(&mut self, ctx: &ShardsContext) -> Result<(), &'static str> {
    self.context = Some(UIContext::default());
    self.instance.warmup(ctx);
    self.parents.warmup(ctx);

    Ok(())
  }

  pub fn cleanup(&mut self) -> Result<(), &'static str> {
    self.parents.cleanup();
    self.instance.cleanup();
    self.context = None;
    Ok(())
  }

  fn clear_unused_dnd_state(&mut self) {
    let ui_ctx = self.context.as_ref().unwrap();
    let mut dnd_value = ui_ctx.dnd_value.borrow_mut();
    if !dnd_value.0.is_none() {
      if !ui_ctx
        .egui_ctx
        .memory(|mem| mem.is_anything_being_dragged())
      {
        let v_empty = Var::default();
        dnd_value.assign(&v_empty);
      }
    }
  }

  pub fn activate(
    &mut self,
    egui_input: &egui_Input,
    contents: &Shards,
    shards_context: &ShardsContext,
    input: &Var,
  ) -> Result<Var, &'static str> {
    self.clear_unused_dnd_state();

    let ui_ctx = self.context.as_ref().unwrap();
    let raw_input = crate::bindings::translate_raw_input(egui_input);
    match raw_input {
      Err(_error) => {
        shlog_debug!("Input translation error: {:?}", _error);
        Err("Input translation error")
      }
      Ok(raw_input) => {
        let draw_scale = raw_input.pixels_per_point.unwrap_or(1.0);

        let mut error: Option<&str> = None;
        let egui_output = ui_ctx.egui_ctx.run(raw_input, |ctx| {
          error = (|| -> Result<(), &str> {
            // Push empty parent UI in case this context is nested inside another UI
            let wire_state: WireState = util::with_none_var(&mut self.parents, || {
              let mut _output = Var::default();
              util::with_object_stack_var(&mut self.instance, ui_ctx, &EGUI_CTX_TYPE, || {
                Ok(unsafe {
                  (*Core).runShards.unwrap()(
                    *contents,
                    shards_context as *const _ as *mut _,
                    input,
                    &mut _output,
                  )
                  .into()
                })
              })
            })?;

            if wire_state == WireState::Error {
              return Err("Failed to activate UI contents");
            }

            Ok(())
          })()
          .err();
        });

        if let Some(e) = error {
          return Err(e);
        }

        self.full_output = Some(make_native_full_output(
          &ui_ctx.egui_ctx,
          egui_output,
          draw_scale,
        )?);
        Ok(*input)
      }
    }
  }

  pub fn get_egui_output(&self) -> *const egui_FullOutput {
    return &self.full_output.as_ref().unwrap().full_output;
  }
}

mod native {
  use crate::bindings::{egui_FullOutput, egui_Input};

  use super::EguiHost;
  use shards::{
    shardsc::{self, SHVar, Shards},
    types::Context,
  };

  #[no_mangle]
  unsafe extern "C" fn egui_createHost() -> *mut EguiHost {
    Box::into_raw(Box::new(EguiHost::default()))
  }

  #[no_mangle]
  unsafe extern "C" fn egui_destroyHost(ptr: *mut EguiHost) {
    drop(Box::from_raw(ptr))
  }

  #[no_mangle]
  unsafe extern "C" fn egui_hostGetExposedTypeInfo(
    ptr: *mut EguiHost,
    out_info: *mut shardsc::SHExposedTypesInfo,
  ) {
    (*out_info) = (&(*ptr).exposed).into();
  }

  #[no_mangle]
  unsafe extern "C" fn egui_hostWarmup(ptr: *mut EguiHost, ctx: &Context) {
    (*ptr).warmup(&ctx).unwrap();
  }

  #[no_mangle]
  unsafe extern "C" fn egui_hostCleanup(ptr: *mut EguiHost) {
    (*ptr).cleanup().unwrap();
  }

  #[no_mangle]
  unsafe extern "C" fn egui_hostActivate(
    ptr: *mut EguiHost,
    egui_input: *const egui_Input,
    contents: &Shards,
    context: &Context,
    input: &SHVar,
    output: *mut SHVar,
  ) -> *const u8 {
    match (*ptr).activate(&*egui_input, &contents, &context, &input) {
      Ok(result) => {
        *output = result;
        std::ptr::null()
      }
      Err(str) => str.as_ptr(),
    }
  }

  #[no_mangle]
  unsafe extern "C" fn egui_hostGetOutput(ptr: *const EguiHost) -> *const egui_FullOutput {
    &(*ptr)
      .full_output
      .as_ref()
      .expect("Expected egui output")
      .full_output
  }
}
