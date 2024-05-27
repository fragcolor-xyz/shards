pub use naga_native as nn;
use wgn::utils::ptr_into_label;
pub use wgpu_native as wgn;
pub use wgpu_native::native as wgnn;

use std::ffi::{c_void, CString};
use std::os::raw::c_char;
use std::sync::Arc;
use wgc::gfx_select;
use wgpu_core as wgc;
use wgpu_types as wgt;

#[cfg(feature = "tracy")]
mod tracy_impl {
  use tracy_client;
  #[no_mangle]
  pub unsafe extern "C" fn gfxTracyInit() {
    tracy_client::Client::start();
  }
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct GFXShaderModuleDescriptor {
  pub label: *const c_char,
  pub wgsl: *const c_char,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct GFXShaderModuleResult {
  pub module: wgnn::WGPUShaderModule,
  pub error: *const c_char,
}

#[no_mangle]
pub unsafe extern "C" fn gfxDeviceCreateShaderModule(
  device_: wgnn::WGPUDevice,
  descriptor: Option<&GFXShaderModuleDescriptor>,
  callback: extern "C" fn(*const GFXShaderModuleResult, *mut c_void),
  user_data: *mut c_void,
) {
  let (device, context) = {
    let device = device_.as_ref().expect("invalid device");
    (device.id, &device.context)
  };

  let descriptor = descriptor.expect("invalid descriptor");

  let label = ptr_into_label(descriptor.label);

  let desc = wgc::pipeline::ShaderModuleDescriptor {
    label: label,
    shader_bound_checks: wgt::ShaderBoundChecks::default(),
  };

  let cstr = std::ffi::CStr::from_ptr(descriptor.wgsl);

  let (id, error) = gfx_select!(device => context.device_create_shader_module(device, &desc,
        wgc::pipeline::ShaderModuleSource::Wgsl(cstr.to_string_lossy()), None));

  if let Some(error) = error {
    let err_str = format!("{}\n{:?}", error, error);
    let err_str = CString::new(err_str).expect("Invalid error string");
    let result = GFXShaderModuleResult {
      module: Arc::into_raw(Arc::new(wgn::WGPUShaderModuleImpl {
        context: context.clone(),
        id: Some(id),
      })),
      error: err_str.as_ptr(),
    };
    callback(&result, user_data);
  } else {
    let result = GFXShaderModuleResult {
      module: Arc::into_raw(Arc::new(wgn::WGPUShaderModuleImpl {
        context: context.clone(),
        id: Some(id),
      })),
      error: std::ptr::null(),
    };
    callback(&result, user_data);
  }
}
