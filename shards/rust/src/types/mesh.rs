/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

//! Mesh types and implementations.

use super::*;
use crate::core::Core;
use crate::shardsc::{SHComposeResult, SHMeshRef, SHTypeDesc, SHWireRef};
use crate::SHStringWithLen;
use std::os::raw::c_char;

/// A single type rendered for a compose diagnostic: a canonical string plus the
/// top-level basic type id (SHType value, -1 when unknown).
#[derive(Debug, Clone)]
pub struct DiagType {
  pub name: std::string::String,
  pub basic_type: i32,
}

/// A structured, machine-readable compose-time diagnostic (see `shards check`).
/// `kind` matches the C `SHDiagnosticKind` enum value.
#[derive(Debug, Clone)]
pub struct ComposeDiagnostic {
  pub kind: u8,
  pub line: u32,
  pub column: u32,
  pub file: std::string::String,
  pub shard_name: std::string::String,
  pub message: std::string::String,
  pub actual: Option<DiagType>,
  pub expected: Vec<DiagType>,
  pub param_index: i32,
}

/// Result of a compose-only check: success flag, the human-readable error/stack
/// trace, and the structured diagnostics.
#[derive(Debug, Clone, Default)]
pub struct CheckResult {
  pub failed: bool,
  pub error: std::string::String,
  pub error_stack_trace: std::string::String,
  pub diagnostics: Vec<ComposeDiagnostic>,
}

unsafe fn read_type_desc(d: &SHTypeDesc) -> Option<DiagType> {
  if d.name.string.is_null() || d.name.len == 0 {
    return None;
  }
  Some(DiagType {
    name: d.name.str().to_string(),
    basic_type: d.basicType,
  })
}

unsafe fn read_check_result(r: &SHComposeResult) -> CheckResult {
  let mut diagnostics = Vec::new();
  if !r.diagnostics.is_null() && r.numDiagnostics > 0 {
    let slice = std::slice::from_raw_parts(r.diagnostics, r.numDiagnostics as usize);
    for d in slice {
      let expected = if d.expected.is_null() || d.numExpected == 0 {
        Vec::new()
      } else {
        std::slice::from_raw_parts(d.expected, d.numExpected as usize)
          .iter()
          .filter_map(|t| read_type_desc(t))
          .collect()
      };
      diagnostics.push(ComposeDiagnostic {
        kind: d.kind,
        line: d.line,
        column: d.column,
        file: d.file.str().to_string(),
        shard_name: d.shardName.str().to_string(),
        message: d.message.str().to_string(),
        actual: read_type_desc(&d.actual),
        expected,
        param_index: d.paramIndex,
      });
    }
  }
  CheckResult {
    failed: r.failed,
    error: r.error.str().to_string(),
    error_stack_trace: r.errorStackTrace.str().to_string(),
    diagnostics,
  }
}

#[repr(transparent)] // force it same size of original
#[derive(Clone)]
pub struct Mesh(pub SHMeshRef);

impl Default for Mesh {
  fn default() -> Self {
    Mesh(unsafe { (*Core).createMesh.unwrap_unchecked()() })
  }
}

impl Drop for Mesh {
  fn drop(&mut self) {
    unsafe { (*Core).destroyMesh.unwrap_unchecked()(self.0) }
  }
}

impl Mesh {
  pub fn compose(&self, wire: WireRef) -> Result<(), std::string::String> {
    let mut error = ClonedVar::default();
    if !unsafe { (*Core).compose.unwrap_unchecked()(self.0, wire.0, &mut error.0) } {
      let error: &str = error.0.as_ref().try_into()?;
      Err(error.to_string())
    } else {
      Ok(())
    }
  }

  /// Compose-only check: composes the wire against this mesh's environment exactly
  /// like a real schedule would, but never schedules/warms/runs. Returns the
  /// structured, machine-readable diagnostics used by `shards check`.
  pub fn compose_check(&self, wire: WireRef) -> CheckResult {
    unsafe {
      let mut result = (*Core).composeForCheck.unwrap_unchecked()(self.0, wire.0);
      let out = read_check_result(&result);
      (*Core).freeComposeResult.unwrap_unchecked()(&mut result);
      out
    }
  }

  pub fn schedule(&mut self, wire: WireRef, compose: bool) {
    unsafe { (*Core).schedule.unwrap_unchecked()(self.0, wire.0, compose) }
  }

  pub fn tick(&mut self) -> bool {
    unsafe { (*Core).tick.unwrap_unchecked()(self.0) }
  }

  pub fn is_empty(&mut self) -> bool {
    unsafe { (*Core).isEmpty.unwrap_unchecked()(self.0) }
  }

  pub fn terminate(&mut self) {
    unsafe { (*Core).terminate.unwrap_unchecked()(self.0) }
  }
}

#[repr(transparent)]
#[derive(Clone)]
pub struct MeshVar(pub ClonedVar);

impl MeshVar {
  pub fn new() -> Self {
    MeshVar(ClonedVar(unsafe {
      (*Core).createMeshVar.unwrap_unchecked()()
    }))
  }

  pub fn mesh_ref(&self) -> SHMeshRef {
    unsafe {
      self
        .0
        .0
        .payload
        .__bindgen_anon_1
        .__bindgen_anon_1
        .objectValue as SHMeshRef
    }
  }

  pub fn compose(&self, wire: WireRef) -> Result<(), std::string::String> {
    let mut error = ClonedVar::default();
    if !unsafe { (*Core).compose.unwrap_unchecked()(self.mesh_ref(), wire.0, &mut error.0) } {
      let error: &str = error.0.as_ref().try_into()?;
      Err(error.to_string())
    } else {
      Ok(())
    }
  }

  pub fn schedule(&mut self, wire: WireRef, compose: bool) {
    unsafe { (*Core).schedule.unwrap_unchecked()(self.mesh_ref(), wire.0, compose) }
  }

  /// Returns false if we had any wire failure
  pub fn tick(&mut self) -> bool {
    unsafe { (*Core).tick.unwrap_unchecked()(self.mesh_ref()) }
  }

  pub fn is_empty(&mut self) -> bool {
    unsafe { (*Core).isEmpty.unwrap_unchecked()(self.mesh_ref()) }
  }

  pub fn terminate(&mut self) {
    unsafe { (*Core).terminate.unwrap_unchecked()(self.mesh_ref()) }
  }

  pub fn set_label(&mut self, str: &str) {
    unsafe {
      let name = SHStringWithLen {
        string: str.as_ptr() as *const c_char,
        len: str.len() as u64,
      };
      (*Core).setMeshLabel.unwrap_unchecked()(self.mesh_ref(), name)
    }
  }
}

// Safety: Mesh can be safely shared between threads
unsafe impl Sync for Mesh {}
