/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2025 Fragcolor Pte. Ltd. */

//! ShardType trait for mapping Rust types to Shards types.
//!
//! This trait enables the simple_shard macro to automatically determine
//! the correct Shards type information from Rust types.

use super::*;
use crate::shardsc::*;

/// Trait for mapping Rust types to Shards types.
///
/// Implement this trait for custom types to enable them to be used
/// with the simple_shard macro for automatic type inference.
pub trait ShardType {
    /// Returns the Shards Type for this Rust type
    fn shards_type() -> Type;
    /// Returns a static reference to the Types vec (for input_types/output_types)
    fn shards_types() -> &'static Types;
    /// Returns the context variable version of this type (for parameters)
    fn shards_var_type() -> Type;
}

// Implement for common primitive types

impl ShardType for i64 {
    fn shards_type() -> Type { common_type::int }
    fn shards_types() -> &'static Types { &INT_TYPES }
    fn shards_var_type() -> Type { common_type::int_var }
}

impl ShardType for i32 {
    fn shards_type() -> Type { common_type::int }
    fn shards_types() -> &'static Types { &INT_TYPES }
    fn shards_var_type() -> Type { common_type::int_var }
}

impl ShardType for f64 {
    fn shards_type() -> Type { common_type::float }
    fn shards_types() -> &'static Types { &FLOAT_TYPES }
    fn shards_var_type() -> Type { common_type::float_var }
}

impl ShardType for f32 {
    fn shards_type() -> Type { common_type::float }
    fn shards_types() -> &'static Types { &FLOAT_TYPES }
    fn shards_var_type() -> Type { common_type::float_var }
}

impl ShardType for bool {
    fn shards_type() -> Type { common_type::bool }
    fn shards_types() -> &'static Types { &BOOL_TYPES }
    fn shards_var_type() -> Type { common_type::bool_var }
}

impl ShardType for std::string::String {
    fn shards_type() -> Type { common_type::string }
    fn shards_types() -> &'static Types { &STRING_TYPES }
    fn shards_var_type() -> Type { common_type::string_var }
}

impl<'a> ShardType for &'a str {
    fn shards_type() -> Type { common_type::string }
    fn shards_types() -> &'static Types { &STRING_TYPES }
    fn shards_var_type() -> Type { common_type::string_var }
}

// Vector types
impl ShardType for (i64, i64) {
    fn shards_type() -> Type { common_type::int2 }
    fn shards_types() -> &'static Types { &INT2_TYPES }
    fn shards_var_type() -> Type { common_type::int2_var }
}

impl ShardType for (i32, i32, i32) {
    fn shards_type() -> Type { common_type::int3 }
    fn shards_types() -> &'static Types { &INT3_TYPES }
    fn shards_var_type() -> Type { common_type::int3_var }
}

impl ShardType for (i32, i32, i32, i32) {
    fn shards_type() -> Type { common_type::int4 }
    fn shards_types() -> &'static Types { &INT4_TYPES }
    fn shards_var_type() -> Type { common_type::int4_var }
}

impl ShardType for (f64, f64) {
    fn shards_type() -> Type { common_type::float2 }
    fn shards_types() -> &'static Types { &FLOAT2_TYPES }
    fn shards_var_type() -> Type { common_type::float2_var }
}

impl ShardType for (f32, f32, f32) {
    fn shards_type() -> Type { common_type::float3 }
    fn shards_types() -> &'static Types { &FLOAT3_TYPES }
    fn shards_var_type() -> Type { common_type::float3_var }
}

impl ShardType for (f32, f32, f32, f32) {
    fn shards_type() -> Type { common_type::float4 }
    fn shards_types() -> &'static Types { &FLOAT4_TYPES }
    fn shards_var_type() -> Type { common_type::float4_var }
}

// Bytes
impl<'a> ShardType for &'a [u8] {
    fn shards_type() -> Type { common_type::bytes }
    fn shards_types() -> &'static Types { &BYTES_TYPES }
    fn shards_var_type() -> Type { common_type::bytes_var }
}

impl ShardType for Vec<u8> {
    fn shards_type() -> Type { common_type::bytes }
    fn shards_types() -> &'static Types { &BYTES_TYPES }
    fn shards_var_type() -> Type { common_type::bytes_var }
}

// None/Unit type
impl ShardType for () {
    fn shards_type() -> Type { common_type::none }
    fn shards_types() -> &'static Types { &NONE_TYPES }
    fn shards_var_type() -> Type { common_type::none } // None doesn't have a var type
}

// Color
impl ShardType for SHColor {
    fn shards_type() -> Type { common_type::color }
    fn shards_types() -> &'static Types { &COLOR_TYPES }
    fn shards_var_type() -> Type { common_type::color_var }
}
