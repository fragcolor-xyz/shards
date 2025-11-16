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

// Re-export all the type constants and FRAG_CC from table module
pub use table::{
    FRAG_CC,
    // Vec<Type> constants
    ANY_TYPES, WIRE_TYPES, ANYS_TYPES, ANY_TABLE_VAR_TYPES, ANY_TABLE_TYPES,
    SEQ_OF_ANY_TABLE_TYPES, NONE_TYPES, STRING_TYPES, STRINGS_TYPES,
    SEQ_OF_STRINGS_TYPES, SEQ_OF_STRINGS_OR_SEQ_OF_BYTES_TYPES,
    SEQ_OF_STRING_OR_BYTE_TYPES, COLOR_TYPES, INT_TYPES, INT2_TYPES,
    INT3_TYPES, INT4_TYPES, INT16_TYPES, FLOAT_TYPES, FLOAT2_TYPES,
    FLOAT3_TYPES, FLOAT4_TYPES, BOOL_TYPES, BYTES_TYPES, AUDIO_TYPES,
    IMAGE_TYPES, SEQ_OF_INT_TYPES, SEQ_OF_INT_OR_FLOAT_TYPES,
    SEQ_OF_FLOAT_TYPES, SEQ_OF_SEQ_OF_INT_TYPES, SEQ_OF_SEQ_OF_FLOAT_TYPES,
    BYTES_OR_STRING_TYPES, FLOAT4X4_TYPES, FLOAT4X4orS_TYPES,
    FLOAT3X3_TYPES, FLOAT4X2_TYPES, ENUM_TYPES, ENUMS_TYPES,
    SHARDS_OR_NONE_TYPES, SEQ_OF_SHARDS_TYPES, SEQ_OF_SEQ_OF_ANY_TYPES,
    // &[Type] slice constants
    INT_TYPES_SLICE, INT_OR_NONE_TYPES_SLICE, INT2_TYPES_SLICE,
    FLOAT_TYPES_SLICE, FLOAT_OR_NONE_TYPES_SLICE, FLOAT2_TYPES_SLICE,
    FLOAT3_TYPES_SLICE, BOOL_TYPES_SLICE, BOOL_OR_NONE_SLICE,
    BOOL_OR_VAR_SLICE, BOOL_VAR_OR_NONE_SLICE, STRING_TYPES_SLICE,
    STRING_OR_NONE_SLICE, STRINGS_OR_NONE_SLICE, STRING_VAR_OR_NONE_SLICE,
    ANY_TABLE_VAR_NONE_SLICE,
};
