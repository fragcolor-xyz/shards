pub use wgpu_native as wgn;
pub use wgpu_native::native as wgnn;
pub use naga_native as nn;

use std::borrow::Cow;
use std::ffi::{c_void, CString};
use std::os::raw::c_char;
use wgc::gfx_select;
use wgn::native::{IntoHandleWithContext, UnwrapId};
use wgn::{make_slice, OwnedLabel};
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
  device: wgnn::WGPUDevice,
  descriptor: Option<&GFXShaderModuleDescriptor>,
  callback: extern "C" fn(*const GFXShaderModuleResult, *mut c_void),
  user_data: *mut c_void,
) {
  let (device, context) = device.unwrap_handle();
  let descriptor = descriptor.expect("invalid descriptor");

  let label = wgn::OwnedLabel::new(descriptor.label);

  let desc = wgc::pipeline::ShaderModuleDescriptor {
    label: label.as_cow(),
    shader_bound_checks: wgt::ShaderBoundChecks::default(),
  };

  let cstr = std::ffi::CStr::from_ptr(descriptor.wgsl);

  let (id, error) = gfx_select!(device => context.device_create_shader_module(device, &desc,
        wgc::pipeline::ShaderModuleSource::Wgsl(cstr.to_string_lossy()), ()));

  if let Some(error) = error {
    let err_str = format!("{}\n{:?}", error, error);
    let err_str = CString::new(err_str).expect("Invalid error string");
    let result = GFXShaderModuleResult {
      module: id.into_handle_with_context(context),
      error: err_str.as_ptr(),
    };
    callback(&result, user_data);
  } else {
    let result = GFXShaderModuleResult {
      module: id.into_handle_with_context(context),
      error: std::ptr::null(),
    };
    callback(&result, user_data);
  }
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct GFXRenderPipelineResult {
  pub pipeline: wgnn::WGPURenderPipeline,
  pub error: *const c_char,
}

#[no_mangle]
pub unsafe extern "C" fn gfxDeviceCreateRenderPipeline(
  device: wgnn::WGPUDevice,
  descriptor: Option<&wgnn::WGPURenderPipelineDescriptor>,
  callback: extern "C" fn(*const GFXRenderPipelineResult, *mut c_void),
  user_data: *mut c_void,
) {
  let (device, context) = device.unwrap_handle();
  let descriptor = descriptor.expect("invalid descriptor");

  let desc = wgc::pipeline::RenderPipelineDescriptor {
    label: OwnedLabel::new(descriptor.label).into_cow(),
    layout: descriptor.layout.as_option(),
    vertex: wgc::pipeline::VertexState {
      stage: wgc::pipeline::ProgrammableStageDescriptor {
        module: descriptor
          .vertex
          .module
          .as_option()
          .expect("invalid vertex shader module for render pipeline descriptor"),
        entry_point: OwnedLabel::new(descriptor.vertex.entryPoint)
          .into_cow()
          .expect("Entry point not provided"),
      },
      buffers: Cow::Owned(
        make_slice(
          descriptor.vertex.buffers,
          descriptor.vertex.bufferCount as usize,
        )
        .iter()
        .map(|buffer| wgc::pipeline::VertexBufferLayout {
          array_stride: buffer.arrayStride,
          step_mode: match buffer.stepMode {
            wgnn::WGPUVertexStepMode_Vertex => wgt::VertexStepMode::Vertex,
            wgnn::WGPUVertexStepMode_Instance => wgt::VertexStepMode::Instance,
            x => panic!("Unknown step mode {x}"),
          },
          attributes: Cow::Owned(
            make_slice(buffer.attributes, buffer.attributeCount as usize)
              .iter()
              .map(|attribute| wgt::VertexAttribute {
                format: wgn::conv::map_vertex_format(attribute.format)
                  .expect("Vertex Format must be defined"),
                offset: attribute.offset,
                shader_location: attribute.shaderLocation,
              })
              .collect(),
          ),
        })
        .collect(),
      ),
    },
    primitive: wgt::PrimitiveState {
      topology: wgn::conv::map_primitive_topology(descriptor.primitive.topology),
      strip_index_format: wgn::conv::map_index_format(descriptor.primitive.stripIndexFormat).ok(),
      front_face: match descriptor.primitive.frontFace {
        wgnn::WGPUFrontFace_CCW => wgt::FrontFace::Ccw,
        wgnn::WGPUFrontFace_CW => wgt::FrontFace::Cw,
        _ => panic!("Front face not provided"),
      },
      cull_mode: match descriptor.primitive.cullMode {
        wgnn::WGPUCullMode_Front => Some(wgt::Face::Front),
        wgnn::WGPUCullMode_Back => Some(wgt::Face::Back),
        _ => None,
      },
      unclipped_depth: false, // todo: fill this via extras
      polygon_mode: wgt::PolygonMode::Fill,
      conservative: false,
    },
    depth_stencil: descriptor
      .depthStencil
      .as_ref()
      .map(|desc| wgt::DepthStencilState {
        format: wgn::conv::map_texture_format(desc.format)
          .expect("Texture format must be defined in DepthStencilState"),
        depth_write_enabled: desc.depthWriteEnabled,
        depth_compare: wgn::conv::map_compare_function(desc.depthCompare).unwrap(),
        stencil: wgt::StencilState {
          front: wgn::conv::map_stencil_face_state(desc.stencilFront),
          back: wgn::conv::map_stencil_face_state(desc.stencilBack),
          read_mask: desc.stencilReadMask,
          write_mask: desc.stencilWriteMask,
        },
        bias: wgt::DepthBiasState {
          constant: desc.depthBias,
          slope_scale: desc.depthBiasSlopeScale,
          clamp: desc.depthBiasClamp,
        },
      }),
    multisample: wgt::MultisampleState {
      count: descriptor.multisample.count,
      mask: descriptor.multisample.mask as u64,
      alpha_to_coverage_enabled: descriptor.multisample.alphaToCoverageEnabled,
    },
    fragment: descriptor
      .fragment
      .as_ref()
      .map(|fragment| wgc::pipeline::FragmentState {
        stage: wgc::pipeline::ProgrammableStageDescriptor {
          module: fragment
            .module
            .as_option()
            .expect("invalid fragment shader module for render pipeline descriptor"),
          entry_point: OwnedLabel::new(fragment.entryPoint)
            .into_cow()
            .expect("Entry point not provided"),
        },
        targets: Cow::Owned(
          make_slice(fragment.targets, fragment.targetCount as usize)
            .iter()
            .map(|color_target| {
              wgn::conv::map_texture_format(color_target.format).map(|format| {
                wgt::ColorTargetState {
                  format,
                  blend: color_target.blend.as_ref().map(|blend| wgt::BlendState {
                    color: wgn::conv::map_blend_component(blend.color),
                    alpha: wgn::conv::map_blend_component(blend.alpha),
                  }),
                  write_mask: wgt::ColorWrites::from_bits(color_target.writeMask).unwrap(),
                }
              })
            })
            .collect(),
        ),
      }),
    multiview: None,
  };

  let implicit_pipeline_ids = match desc.layout {
    Some(_) => None,
    None => Some(wgc::device::ImplicitPipelineIds {
      root_id: (),
      group_ids: &[(); wgc::MAX_BIND_GROUPS],
    }),
  };

  let (id, error) = gfx_select!(device => context.device_create_render_pipeline(device, &desc, (), implicit_pipeline_ids));
  let handle = id.into_handle_with_context(context);
  if let Some(error) = error {
    let err_str = CString::new(error.to_string()).expect("Invalid error string");
    let result = GFXRenderPipelineResult {
      pipeline: handle,
      error: err_str.as_ptr(),
    };
    callback(&result, user_data);
  } else {
    let result = GFXRenderPipelineResult {
      pipeline: handle,
      error: std::ptr::null(),
    };
    callback(&result, user_data);
  }
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct GFXComputePipelineResult {
  pub pipeline: wgnn::WGPUComputePipeline,
  pub error: *const c_char,
}

#[no_mangle]
pub unsafe extern "C" fn gfxDeviceCreateComputePipeline(
  device: wgnn::WGPUDevice,
  descriptor: Option<&wgnn::WGPUComputePipelineDescriptor>,
  callback: extern "C" fn(*const GFXComputePipelineResult, *mut c_void),
  user_data: *mut c_void,
) {
  let (device, context) = device.unwrap_handle();
  let descriptor = descriptor.expect("invalid descriptor");

  let stage = wgc::pipeline::ProgrammableStageDescriptor {
    module: descriptor
      .compute
      .module
      .as_option()
      .expect("invalid shader module for compute pipeline descriptor"),
    entry_point: OwnedLabel::new(descriptor.compute.entryPoint)
      .into_cow()
      .expect("invalid entry point for compute pipeline descriptor"),
  };
  let desc = wgc::pipeline::ComputePipelineDescriptor {
    label: OwnedLabel::new(descriptor.label).into_cow(),
    layout: descriptor.layout.as_option(),
    stage,
  };

  let implicit_pipeline_ids = match desc.layout {
    Some(_) => None,
    None => Some(wgc::device::ImplicitPipelineIds {
      root_id: (),
      group_ids: &[(); wgc::MAX_BIND_GROUPS],
    }),
  };

  let (id, error) = gfx_select!(device => context.device_create_compute_pipeline(device, &desc, (), implicit_pipeline_ids));
  let handle = id.into_handle_with_context(context);

  if let Some(error) = error {
    let err_str = CString::new(error.to_string()).expect("Invalid error string");
    let result = GFXComputePipelineResult {
      pipeline: handle,
      error: err_str.as_ptr(),
    };
    callback(&result, user_data);
  } else {
    let result = GFXComputePipelineResult {
      pipeline: handle,
      error: std::ptr::null(),
    };
    callback(&result, user_data);
  }
}
