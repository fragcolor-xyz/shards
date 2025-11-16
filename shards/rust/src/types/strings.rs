/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

//! String utilities and ExposedTypesIterator.

use super::*;
use crate::core::Core;
use crate::shardsc::*;
use std::ffi::CStr;
use std::slice;

pub type OptionalStrings = Vec<OptionalString>;

impl From<&OptionalStrings> for SHOptionalStrings {
  fn from(vec: &OptionalStrings) -> Self {
    SHOptionalStrings {
      elements: vec.as_ptr() as *mut SHOptionalString,
      len: vec.len() as u32,
      cap: 0,
    }
  }
}

pub struct ExposedTypesIterator {
  elements: *mut SHExposedTypeInfo,
  length: usize,
  index: usize,
}

impl Iterator for ExposedTypesIterator {
  type Item = SHExposedTypeInfo;

  fn next(&mut self) -> Option<Self::Item> {
    if self.index < self.length {
      Some({
        let ret_index = self.index;
        self.index += 1;
        if self.length == 0 {
          return None;
        }
        unsafe { slice::from_raw_parts_mut(self.elements, self.length)[ret_index] }
      })
    } else {
      None
    }
  }
}

impl IntoIterator for &SHExposedTypesInfo {
  type Item = SHExposedTypeInfo;
  type IntoIter = ExposedTypesIterator;

  fn into_iter(self) -> Self::IntoIter {
    ExposedTypesIterator {
      elements: self.elements,
      index: 0,
      length: self.len as usize,
    }
  }
}

impl SHExposedTypesInfo {
  pub fn iter(&self) -> ExposedTypesIterator {
    (&self).into_iter()
  }
}

// Strings / SHStrings

pub struct Strings {
  pub s: SHStrings, // Don't derive clone, won't work, it will double free
  owned: bool,
}

impl Drop for Strings {
  fn drop(&mut self) {
    if self.owned {
      unsafe {
        (*Core).stringsFree.unwrap_unchecked()(&self.s as *const SHStrings as *mut SHStrings);
      }
    }
  }
}

impl Strings {
  pub const fn new() -> Self {
    Self {
      s: SHStrings {
        elements: core::ptr::null_mut(),
        len: 0,
        cap: 0,
      },
      owned: true,
    }
  }

  pub fn set_len(&mut self, len: usize) {
    unsafe {
      (*Core).stringsResize.unwrap_unchecked()(
        &self.s as *const SHStrings as *mut SHStrings,
        len.try_into().unwrap(),
      );
    }
  }

  pub fn push(&mut self, value: &str) {
    let str = value.as_ptr() as *const std::os::raw::c_char;
    unsafe {
      (*Core).stringsPush.unwrap_unchecked()(&self.s as *const SHStrings as *mut SHStrings, &str);
    }
  }

  pub fn insert(&mut self, index: usize, value: &str) {
    let str = value.as_ptr() as *const std::os::raw::c_char;
    unsafe {
      (*Core).stringsInsert.unwrap_unchecked()(
        &self.s as *const SHStrings as *mut SHStrings,
        index.try_into().unwrap(),
        &str,
      );
    }
  }

  pub fn len(&self) -> usize {
    self.s.len.try_into().unwrap()
  }

  pub fn is_empty(&self) -> bool {
    self.s.len == 0
  }

  pub fn pop(&mut self) -> Option<&str> {
    unsafe {
      if !self.is_empty() {
        let v =
          (*Core).stringsPop.unwrap_unchecked()(&self.s as *const SHStrings as *mut SHStrings);
        Some(CStr::from_ptr(v).to_str().unwrap())
      } else {
        None
      }
    }
  }

  pub fn clear(&mut self) {
    unsafe {
      (*Core).stringsResize.unwrap_unchecked()(&self.s as *const SHStrings as *mut SHStrings, 0);
    }
  }
}

impl Default for Strings {
  fn default() -> Self {
    Strings::new()
  }
}

impl AsRef<Strings> for Strings {
  #[inline(always)]
  fn as_ref(&self) -> &Strings {
    self
  }
}

