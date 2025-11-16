/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

//! Type system for Shards.
//!
//! This module contains all the types, traits, and implementations for working with
//! Shards variables, types, and runtime constructs.

// Re-export the cstr! macro at the module level
#[macro_export]
macro_rules! cstr {
  ($text:expr) => {
    concat!($text, "\0")
  };
}

// Declare all submodules
pub mod common;
pub mod mesh;
pub mod wire;
pub mod shard_ref;
pub mod metadata;
pub mod refcounted;
pub mod param;
pub mod strings;
pub mod seq;
pub mod table;

// Re-export common types that are used everywhere
pub use common::*;

// Re-export from submodules
pub use mesh::{Mesh, MeshVar};
pub use wire::{
  Wire, WireRef, WireState, EnumInfoId, ObjectInfoId,
  get_enum_info, get_object_info, find_object_type_id, find_object_type_vendor_type_pair,
};
pub use shard_ref::{ShardRef, AutoShardRef};
pub use metadata::*;
pub use refcounted::*;
pub use param::{ParamVar, ShardsVar};
pub use strings::{Strings, OptionalStrings, ExposedTypesIterator};
pub use seq::{Seq, SeqVar, AutoSeqVar, SeqVarIterator, SeqIterator};
pub use table::{Table, TableVar, AutoTableVar, TableIterator};
