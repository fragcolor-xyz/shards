/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

//! Sequence types (SeqVar, AutoSeqVar, Seq) and iterators.

use super::*;
use crate::core::{cloneVar, destroyVar, Core};
use crate::shardsc::*;
use std::fmt::Debug;
use std::mem::transmute;
use std::ops::{Index, IndexMut};

#[derive(Copy, Clone)]
pub struct SeqVar(pub Var);

/// A wrapper around `SeqVar` that automatically destroys the variable when it goes out of scope.
#[repr(transparent)] // force it same size of original
pub struct AutoSeqVar(pub SeqVar);

impl Drop for AutoSeqVar {
  fn drop(&mut self) {
    destroyVar(&mut self.0 .0);
  }
}

impl AutoSeqVar {
  /// Creates a new `AutoSeqVar`.
  pub fn new() -> AutoSeqVar {
    AutoSeqVar(SeqVar::new())
  }

  /// Extracts the `Var` from the `AutoSeqVar` and leaks it.
  ///
  /// This function destroys the `AutoSeqVar` and returns the `Var` contained within it.
  /// The `Var` will not be destroyed when it goes out of scope.
  pub fn leak(&mut self) -> Var {
    std::mem::replace(&mut self.0 .0, Var::default())
  }
}


pub struct SeqVarIterator {
  s: SeqVar,
  i: u32,
}

impl Iterator for SeqVarIterator {
  fn next(&mut self) -> Option<Self::Item> {
    unsafe {
      let res = if self.i < self.s.0.payload.__bindgen_anon_1.seqValue.len {
        Some(
          *self
            .s
            .0
            .payload
            .__bindgen_anon_1
            .seqValue
            .elements
            .offset(self.i.try_into().unwrap()),
        )
      } else {
        None
      };
      self.i += 1;
      res
    }
  }
  type Item = Var;
}

impl DoubleEndedIterator for SeqVarIterator {
  fn next_back(&mut self) -> Option<Self::Item> {
    unsafe {
      let res = if self.i < self.s.0.payload.__bindgen_anon_1.seqValue.len {
        Some(
          *self.s.0.payload.__bindgen_anon_1.seqValue.elements.offset(
            (self.s.0.payload.__bindgen_anon_1.seqValue.len - self.i - 1)
              .try_into()
              .unwrap(),
          ),
        )
      } else {
        None
      };
      self.i += 1;
      res
    }
  }
}

impl Index<usize> for SeqVar {
  #[inline(always)]
  fn index(&self, idx: usize) -> &Self::Output {
    let idx_u32: u32 = idx.try_into().unwrap();
    let len = unsafe { self.0.payload.__bindgen_anon_1.seqValue.len };
    if idx_u32 < len {
      unsafe {
        &*self
          .0
          .payload
          .__bindgen_anon_1
          .seqValue
          .elements
          .offset(idx.try_into().unwrap())
      }
    } else {
      panic!("Index out of range");
    }
  }
  type Output = Var;
}

impl IndexMut<usize> for SeqVar {
  #[inline(always)]
  fn index_mut(&mut self, idx: usize) -> &mut Self::Output {
    let idx_u32: u32 = idx.try_into().unwrap();
    let len = unsafe { self.0.payload.__bindgen_anon_1.seqValue.len };
    if idx_u32 < len {
      unsafe {
        &mut *self
          .0
          .payload
          .__bindgen_anon_1
          .seqValue
          .elements
          .offset(idx.try_into().unwrap())
      }
    } else {
      panic!("Index out of range");
    }
  }
}

impl SeqVar {
  #[inline(always)]
  pub(crate) fn new() -> SeqVar {
    SeqVar {
      0: Var {
        payload: SHVarPayload {
          __bindgen_anon_1: SHVarPayload__bindgen_ty_1 {
            seqValue: SHSeq {
              elements: 0 as *mut SHVar,
              len: 0,
              cap: 0,
            },
          },
        },
        valueType: SHType_Seq,
        ..Default::default()
      },
    }
  }

  #[inline(always)]
  /// Creates a new `SeqVar` that IS NOT DROPPED when it goes out of scope.
  /// The resulting variable should be wrapped inside a `ClonedVar(v)` or `destroyVar` should be called.
  pub fn leaking_new() -> SeqVar {
    Self::new()
  }

  #[inline(always)]
  pub fn wrap(var: Var) -> SeqVar {
    SeqVar { 0: var }
  }

  #[inline(always)]
  pub fn set_len(&mut self, len: usize) {
    unsafe {
      (*Core).seqResize.unwrap_unchecked()(
        &self.0.payload.__bindgen_anon_1.seqValue as *const SHSeq as *mut SHSeq,
        len.try_into().unwrap(),
      );
    }
  }

  #[inline(always)]
  pub fn push(&mut self, value: &Var) {
    // we need to clone to own the memory shards side
    let idx = self.len();
    self.set_len(idx + 1);
    cloneVar(&mut self[idx], &value);
  }

  #[inline(always)]
  pub fn emplace(&mut self, value: ClonedVar) {
    // we need to clone to own the memory shards side
    let idx = self.len();
    self.set_len(idx + 1);
    let v = &mut self[idx];
    *v = value.0;
    // now make sure value is not dropped
    std::mem::forget(value);
  }

  #[inline(always)]
  pub fn emplace_table(&mut self, value: AutoTableVar) {
    // we need to clone to own the memory shards side
    let idx = self.len();
    self.set_len(idx + 1);
    let v = &mut self[idx];
    *v = value.0 .0;
    // now make sure value is not dropped
    std::mem::forget(value);
  }

  #[inline(always)]
  pub fn emplace_seq(&mut self, value: AutoSeqVar) {
    // we need to clone to own the memory shards side
    let idx = self.len();
    self.set_len(idx + 1);
    let v = &mut self[idx];
    *v = value.0 .0;
    // now make sure value is not dropped
    std::mem::forget(value);
  }

  #[inline(always)]
  pub fn insert(&mut self, index: usize, value: &Var) {
    // we need to clone to own the memory shards side
    let mut tmp = SHVar::default();
    cloneVar(&mut tmp, &value);
    unsafe {
      (*Core).seqInsert.unwrap_unchecked()(
        &self.0.payload.__bindgen_anon_1.seqValue as *const SHSeq as *mut SHSeq,
        index.try_into().unwrap(),
        &tmp,
      );
    }
  }

  #[inline(always)]
  pub fn len(&self) -> usize {
    unsafe {
      self
        .0
        .payload
        .__bindgen_anon_1
        .seqValue
        .len
        .try_into()
        .unwrap()
    }
  }

  #[inline(always)]
  pub fn is_empty(&self) -> bool {
    self.len() == 0
  }

  #[inline(always)]
  pub fn pop(&mut self) -> Option<ClonedVar> {
    unsafe {
      if !self.is_empty() {
        let v = (*Core).seqPop.unwrap_unchecked()(
          &self.0.payload.__bindgen_anon_1.seqValue as *const SHSeq as *mut SHSeq,
        );
        Some(transmute(v))
      } else {
        None
      }
    }
  }

  #[inline(always)]
  pub fn remove(&mut self, index: usize) {
    unsafe {
      (*Core).seqSlowDelete.unwrap_unchecked()(
        &self.0.payload.__bindgen_anon_1.seqValue as *const SHSeq as *mut SHSeq,
        index.try_into().unwrap(),
      );
    }
  }

  #[inline(always)]
  pub fn remove_fast(&mut self, index: usize) {
    unsafe {
      (*Core).seqFastDelete.unwrap_unchecked()(
        &self.0.payload.__bindgen_anon_1.seqValue as *const SHSeq as *mut SHSeq,
        index.try_into().unwrap(),
      );
    }
  }

  #[inline(always)]
  pub fn clear(&mut self) {
    unsafe {
      (*Core).seqResize.unwrap_unchecked()(
        &self.0.payload.__bindgen_anon_1.seqValue as *const SHSeq as *mut SHSeq,
        0,
      );
    }
  }

  #[inline(always)]
  pub fn iter(&self) -> SeqVarIterator {
    SeqVarIterator {
      s: *self,
      i: 0,
    }
  }

  #[inline(always)]
  pub fn next_mut(&mut self) -> &mut Var {
    let idx = self.len();
    self.set_len(idx + 1);
    &mut self[idx]
  }
}

// Seq / SHSeq

pub struct Seq {
  s: SHSeq, // Don't derive clone, won't work, it will double free
  owned: bool,
}

impl Drop for Seq {
  fn drop(&mut self) {
    if self.owned {
      unsafe {
        (*Core).seqFree.unwrap_unchecked()(&self.s as *const SHSeq as *mut SHSeq);
      }
    }
  }
}

pub struct SeqIterator<'a> {
  s: &'a Seq,
  i: u32,
}

impl<'a> Iterator for SeqIterator<'a> {
  fn next(&mut self) -> Option<Self::Item> {
    let res = if self.i < self.s.s.len {
      unsafe { Some(&*self.s.s.elements.offset(self.i.try_into().unwrap())) }
    } else {
      None
    };
    self.i += 1;
    res
  }
  type Item = &'a Var;
}

impl<'a> DoubleEndedIterator for SeqIterator<'a> {
  fn next_back(&mut self) -> Option<Self::Item> {
    let res = if self.i < self.s.s.len {
      unsafe {
        Some(
          &*self
            .s
            .s
            .elements
            .offset((self.s.s.len - self.i - 1).try_into().unwrap()),
        )
      }
    } else {
      None
    };
    self.i += 1;
    res
  }
}

impl Index<usize> for SHSeq {
  #[inline(always)]
  fn index(&self, idx: usize) -> &Self::Output {
    let idx_u32: u32 = idx.try_into().unwrap();
    if idx_u32 < self.len {
      unsafe { &*self.elements.offset(idx.try_into().unwrap()) }
    } else {
      panic!("Index out of range");
    }
  }
  type Output = Var;
}

impl Index<usize> for Seq {
  #[inline(always)]
  fn index(&self, idx: usize) -> &Self::Output {
    &self.s[idx]
  }
  type Output = Var;
}

impl IndexMut<usize> for SHSeq {
  #[inline(always)]
  fn index_mut(&mut self, idx: usize) -> &mut Self::Output {
    let idx_u32: u32 = idx.try_into().unwrap();
    if idx_u32 < self.len {
      unsafe { &mut *self.elements.offset(idx.try_into().unwrap()) }
    } else {
      panic!("Index out of range");
    }
  }
}

impl IndexMut<usize> for Seq {
  #[inline(always)]
  fn index_mut(&mut self, idx: usize) -> &mut Self::Output {
    &mut self.s[idx]
  }
}

impl Seq {
  pub const fn new() -> Seq {
    Seq {
      s: SHSeq {
        elements: core::ptr::null_mut(),
        len: 0,
        cap: 0,
      },
      owned: true,
    }
  }

  pub fn set_len(&mut self, len: usize) {
    unsafe {
      (*Core).seqResize.unwrap_unchecked()(
        &self.s as *const SHSeq as *mut SHSeq,
        len.try_into().unwrap(),
      );
    }
  }

  pub fn push(&mut self, value: &Var) {
    // we need to clone to own the memory shards side
    let idx = self.len();
    self.set_len(idx + 1);
    cloneVar(&mut self[idx], &value);
  }

  pub fn insert(&mut self, index: usize, value: &Var) {
    // we need to clone to own the memory shards side
    let mut tmp = SHVar::default();
    cloneVar(&mut tmp, &value);
    unsafe {
      (*Core).seqInsert.unwrap_unchecked()(
        &self.s as *const SHSeq as *mut SHSeq,
        index.try_into().unwrap(),
        &tmp,
      );
    }
  }

  pub fn len(&self) -> usize {
    self.s.len.try_into().unwrap()
  }

  pub fn is_empty(&self) -> bool {
    self.s.len == 0
  }

  pub fn pop(&mut self) -> Option<ClonedVar> {
    unsafe {
      if !self.is_empty() {
        let v = (*Core).seqPop.unwrap_unchecked()(&self.s as *const SHSeq as *mut SHSeq);
        Some(transmute(v))
      } else {
        None
      }
    }
  }

  pub fn remove(&mut self, index: usize) {
    unsafe {
      (*Core).seqSlowDelete.unwrap_unchecked()(
        &self.s as *const SHSeq as *mut SHSeq,
        index.try_into().unwrap(),
      );
    }
  }

  pub fn remove_fast(&mut self, index: usize) {
    unsafe {
      (*Core).seqFastDelete.unwrap_unchecked()(
        &self.s as *const SHSeq as *mut SHSeq,
        index.try_into().unwrap(),
      );
    }
  }

  pub fn clear(&mut self) {
    unsafe {
      (*Core).seqResize.unwrap_unchecked()(&self.s as *const SHSeq as *mut SHSeq, 0);
    }
  }

  pub fn iter(&self) -> SeqIterator<'_> {
    SeqIterator { s: self, i: 0 }
  }
}

impl Default for Seq {
  fn default() -> Self {
    Seq::new()
  }
}

impl Into<SHSeq> for &Seq {
  fn into(self) -> SHSeq {
    self.s
  }
}

impl AsRef<Seq> for Seq {
  #[inline(always)]
  fn as_ref(&self) -> &Seq {
    self
  }
}

impl From<&Seq> for Var {
  #[inline(always)]
  fn from(s: &Seq) -> Self {
    SHVar {
      valueType: SHType_Seq,
      payload: SHVarPayload {
        __bindgen_anon_1: SHVarPayload__bindgen_ty_1 { seqValue: s.s },
      },
      ..Default::default()
    }
  }
}

impl TryFrom<&mut Var> for Seq {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(v: &mut Var) -> Result<Self, Self::Error> {
    // in this case allow None type, we might be a new variable from a Table or Seq
    if v.valueType == SHType_None {
      v.valueType = SHType_Seq;
    }

    if v.valueType != SHType_Seq {
      Err("Expected Seq variable, but casting failed.")
    } else {
      unsafe {
        Ok(Seq {
          s: v.payload.__bindgen_anon_1.seqValue,
          owned: false,
        })
      }
    }
  }
}

impl TryFrom<&Var> for Seq {
  type Error = &'static str;

  #[inline(always)]
  fn try_from(v: &Var) -> Result<Self, Self::Error> {
    if v.valueType != SHType_Seq {
      Err("Expected Seq variable, but casting failed.")
    } else {
      unsafe {
        Ok(Seq {
          s: v.payload.__bindgen_anon_1.seqValue,
          owned: false,
        })
      }
    }
  }
}

