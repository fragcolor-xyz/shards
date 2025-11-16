/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

//! Mesh types and implementations.

use super::*;
use crate::core::Core;
use crate::shardsc::{SHMeshRef, SHWireRef};
use crate::SHStringWithLen;
use std::os::raw::c_char;

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
