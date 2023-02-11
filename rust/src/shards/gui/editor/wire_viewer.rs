/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2023 Fragcolor Pte. Ltd. */

use super::ExtPanel;
use super::WireViewer;
use crate::core::ShardInstance;
use crate::shard::Shard;
use crate::shards::editor::*;
use crate::shards::gui::egui_host::EguiHost;
use crate::shards::gui::util;
use crate::shards::gui::GFX_CONTEXT_TYPE;
use crate::shards::gui::GFX_QUEUE_VAR_TYPES;
use crate::shards::gui::HELP_OUTPUT_EQUAL_INPUT;
use crate::shards::gui::HELP_VALUE_IGNORED;
use crate::shards::gui::INPUT_CONTEXT_TYPE;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::shardsc;
use crate::types::common_type;
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::OptionalString;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Type;
use crate::types::Types;
use crate::types::Var;
use crate::types::WireRef;
use crate::types::ANY_TYPES;
use egui_gfx::*;
use std::borrow::BorrowMut;
use std::cell::RefCell;
use std::collections::HashMap;
use std::ffi::CStr;
use std::rc::Rc;

lazy_static! {
  static ref SPATIAL_CONTEXT_TYPE: Type = unsafe { *super::spatial_getSpatialContextType() };
  static ref WIRE_OR_VAR_TYPES: Vec<Type> = vec![common_type::wire, common_type::wire_var];
  static ref VIEWER_PARAMETERS: Parameters = vec![
    // (
    //   cstr!("Queue"),
    //   shccstr!("The draw queue."),
    //   &GFX_QUEUE_VAR_TYPES[..]
    // )
    //   .into(),
    (cstr!("Wire"), shccstr!("TODO"), &WIRE_OR_VAR_TYPES[..]).into(),
  ];
}

impl<'a> Default for WireViewer<'a> {
  fn default() -> Self {
    Self {
      wire: ParamVar::default(),
      requiring: Vec::new(),
      // ---
      // queue: ParamVar::default(),
      graphics_context: unsafe {
        let mut var = ParamVar::default();
        let name = shardsc::gfx_getGraphicsContextVarName() as shardsc::SHString;
        var.set_name(CStr::from_ptr(name).to_str().unwrap());
        var
      },
      input_context: unsafe {
        let mut var = ParamVar::default();
        let name = shardsc::gfx_getInputContextVarName() as shardsc::SHString;
        var.set_name(CStr::from_ptr(name).to_str().unwrap());
        var
      },
      spatial_context: unsafe {
        let mut var = ParamVar::default();
        let name = super::spatial_getSpatialContextVarName() as shardsc::SHString;
        var.set_name(CStr::from_ptr(name).to_str().unwrap());
        var
      },
      renderer: egui_gfx::Renderer::new(),
      input_translator: egui_gfx::InputTranslator::new(),
      // ---
      graph: Rc::new(RefCell::new(Default::default())),
      panels: Default::default(),
      // node_hosts: Default::default(),
      // node_positions: Default::default(),
    }
  }
}

impl<'a> Shard for WireViewer<'a> {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("WireViewer")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("WireViewer-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "WireViewer"
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    *HELP_VALUE_IGNORED
  }

  fn outputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    *HELP_OUTPUT_EQUAL_INPUT
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&VIEWER_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      // 0 => Ok(self.queue.set_param(value)),
      0 => Ok(self.wire.set_param(value)),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      // 0 => self.queue.get_param(),
      0 => self.wire.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add Graphics context to the list of required variables
    let exp_info = ExposedInfo {
      exposedType: *GFX_CONTEXT_TYPE,
      name: self.graphics_context.get_name(),
      help: cstr!("The graphics context.").into(),
      ..ExposedInfo::default()
    };
    self.requiring.push(exp_info);

    // Add Input context to the list of required variables
    let exp_info = ExposedInfo {
      exposedType: *INPUT_CONTEXT_TYPE,
      name: self.input_context.get_name(),
      help: cstr!("The input context.").into(),
      ..ExposedInfo::default()
    };
    self.requiring.push(exp_info);

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
    // self.queue.warmup(ctx);
    self.wire.warmup(ctx);
    self.graphics_context.warmup(ctx);
    self.input_context.warmup(ctx);
    self.spatial_context.warmup(ctx);

    // FIXME we should recompute the data if the wire changed (or not)
    self.compute_data(ctx)?;

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.spatial_context.cleanup();
    self.input_context.cleanup();
    self.graphics_context.cleanup();
    self.wire.cleanup();
    // self.queue.cleanup();

    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    // if let Some(ui) = util::get_current_parent(self.parents.get())? {
    //   for node_id in self.node_positions.keys() {
    //     let mut _pos = egui::Pos2::default();
    //     let mut _node_rects = HashMap::new();
    //     GraphNodeWidget {
    //       position: &mut _pos,
    //       graph: &mut self.graph,
    //       node_rects: &mut _node_rects,
    //       node_id,
    //       pan: Default::default(),
    //       selected: false,
    //     }
    //     .show(ui);
    //   }

    //   // Always passthrough the input
    //   Ok(*input)
    // } else {
    //   Err("No UI parent")
    // }

    // let egui_input = unsafe {
    //   &*(shardsc::gfx_getEguiWindowInputs(
    //     self.input_translator.as_mut_ptr() as *mut shardsc::gfx_EguiInputTranslator,
    //     self.graphics_context.get(),
    //     self.input_context.get(),
    //     1.0,
    //   ) as *const egui_gfx::egui_Input)
    // };

    // for node_id in self.node_positions.keys() {
    //   if let Some(host) = self.node_hosts.get_mut(node_id) {
    //     host.activate_base(egui_input, |_this, ctx| {
    //       egui::CentralPanel::default().show(ctx, |ui| {
    //         let mut _pos = egui::Pos2::default();
    //         let mut _node_rects = HashMap::new();
    //         GraphNodeWidget {
    //           position: &mut _pos,
    //           graph: &mut self.graph,
    //           node_rects: &mut _node_rects,
    //           node_id,
    //           pan: Default::default(),
    //           selected: false,
    //         }
    //         .show(ui);
    //       });

    //       Ok(())
    //     })?;
    //     let egui_output = host.get_egui_output();
    //     let queue_var = self.queue.get();
    //     unsafe {
    //       let queue = shardsc::gfx_getDrawQueueFromVar(queue_var);
    //       self
    //         .renderer
    //         .render_with_native_output(egui_output, queue as *const egui_gfx::gfx_DrawQueuePtr);
    //     }
    //   }
    // }

    // Always passthrough the input
    Ok(*input)
  }
}

impl<'a> WireViewer<'a> {
  fn compute_data(&mut self, ctx: &Context) -> Result<(), &str> {
    let wire: WireRef = self.wire.get().try_into()?;
    let info = wire.get_wire_info();

    let shards = info.shards;
    let shards: Vec<ShardInstance> =
      unsafe { core::slice::from_raw_parts(shards.elements, shards.len as usize) }
        .iter()
        .map(|ptr| (*ptr).into())
        .collect();

    for s in shards.into_iter() {
      let data: ShardData = s.into();
      let node_id = RefCell::borrow_mut(&self.graph).add_node(data.name.to_string(), data);

      let mut panel = ExtPanel {
        host: EguiHost::default(),
        node_id,
        graph: self.graph.clone(),
      };
      panel.warmup(ctx)?;
      unsafe {
        super::spatial_add_panel(&mut panel, self.spatial_context.get());
      }
      self.panels.insert(node_id, panel);
     

      // self.node_hosts.insert(node_id, host);
      // self.node_positions.insert(node_id, [0.0, 0.0, 0.0]);
    }

    // let data: Vec<ShardData> = shards.into_iter().map(|s| s.into()).collect();
    // shlog_debug!("Data: {:?}", data);

    // // Debug
    // shlog_debug!("Info: {:#?}", info);
    Ok(())
  }
}

impl ExtPanel<'_> {
  fn warmup(&mut self, ctx: &Context) -> Result<(), &'static str> {
    self.host.warmup(ctx)
  }

  fn cleanup(&mut self) -> Result<(), &'static str> {
    self.host.cleanup()
  }
}

#[no_mangle]
unsafe extern "C" fn spatial_render_external_panel(
  ptr: *mut ExtPanel,
  egui_input: *const egui_Input,
) -> *const egui_FullOutput {
  (*ptr).host.activate_base(&*egui_input, |_, ctx| {
    // let mut graph = RefCell::borrow_mut(&(*ptr).graph);
    egui::CentralPanel::default().show(ctx, |ui| {
      // let mut _pos = egui::Pos2::default();
      // let mut _node_rects = HashMap::new();
      // GraphNodeWidget {
      //   position: &mut _pos,
      //   graph: &mut graph,
      //   node_rects: &mut _node_rects,
      //   node_id: (*ptr).node_id,
      //   pan: Default::default(),
      //   selected: false,
      // }
      // .show(ui);
    });

    Ok(())
  }).unwrap();

  (*ptr).host.get_egui_output()
}

#[no_mangle]
unsafe extern "C" fn spatial_get_geometry(
  ptr: *const ExtPanel,
) -> shardsc::shards_spatial_PanelGeometry {
  Default::default()
}
