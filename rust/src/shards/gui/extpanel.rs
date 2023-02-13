use super::egui_host::EguiHost;
use super::ExtPanel;
use crate::shard::Shard;
use crate::shards::gui::ANY_VAR_SLICE;
use crate::shardsc;
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Type;
use crate::types::Types;
use crate::types::Var;
use crate::types::ANY_TYPES;
use std::ffi::CStr;

lazy_static! {
  static ref SPATIAL_CONTEXT_TYPE: Type = unsafe { *shardsc::spatial_getSpatialContextType() };
  static ref EXTPANEL_PARAMETERS: Parameters = vec![
    (cstr!("Transform"), shccstr!("TODO"), ANY_VAR_SLICE,).into(),
    (cstr!("Size"), shccstr!("TODO"), ANY_VAR_SLICE,).into(),
  ];
}

impl Default for ExtPanel {
  fn default() -> Self {
    Self {
      transform: ParamVar::default(),
      size: ParamVar::default(),
      spatial_context: unsafe {
        let mut var = ParamVar::default();
        let name = shardsc::spatial_getSpatialContextVarName() as shardsc::SHString;
        var.set_name(CStr::from_ptr(name).to_str().unwrap());
        var
      },
      requiring: Vec::new(),
      host: EguiHost::default(),
    }
  }
}

impl Shard for ExtPanel {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("Rust.ExtPanel")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("Rust.ExtPanel-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "Rust.ExtPanel"
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn outputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&EXTPANEL_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.transform.set_param(value)),
      1 => Ok(self.size.set_param(value)),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.transform.get_param(),
      1 => self.size.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add Spatial context to the list of required variables
    let exp_info = ExposedInfo {
      exposedType: *SPATIAL_CONTEXT_TYPE,
      name: self.spatial_context.get_name(),
      help: cstr!("The spatial context.").into(),
      ..ExposedInfo::default()
    };
    self.requiring.push(exp_info);

    Some(&self.requiring)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.transform.warmup(ctx);
    self.size.warmup(ctx);
    self.spatial_context.warmup(ctx);
    self.host.warmup(ctx)?;

    unsafe {
      spatial_add_panel(self, self.spatial_context.get());
    }

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    unsafe {
      spatial_remove_panel(self, self.spatial_context.get());
    }

    self.size.cleanup();
    self.transform.cleanup();
    self.host.cleanup()?;
    self.spatial_context.cleanup();

    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    // Always passthrough the input
    Ok(*input)
  }
}

extern "C" {
  fn spatial_add_panel(ptr: *mut ExtPanel, spatialContextVar: &Var);
  fn spatial_remove_panel(ptr: *mut ExtPanel, spatialContextVar: &Var);
  fn spatial_temp_get_geometry_from_transform(
    transform: &Var,
    size: &Var,
  ) -> shardsc::shards_spatial_PanelGeometry;
}

#[no_mangle]
unsafe extern "C" fn spatial_render_external_panel(
  ptr: *mut ExtPanel,
  egui_input: *const egui_gfx::egui_Input,
) -> *const egui_gfx::egui_FullOutput {
  (*ptr)
    .host
    .activate_base(&*egui_input, |_, ctx| {
      egui::CentralPanel::default().show(ctx, |ui| ui.label("Hello from rust!"));

      Ok(())
    })
    .unwrap();

  (*ptr).host.get_egui_output()
}

#[no_mangle]
unsafe extern "C" fn spatial_get_geometry(
  ptr: *const ExtPanel,
) -> shardsc::shards_spatial_PanelGeometry {
  let transform = (*ptr).transform.get();
  let size = (*ptr).size.get();

  spatial_temp_get_geometry_from_transform(transform, size)
}
