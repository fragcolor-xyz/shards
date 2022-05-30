/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */
use crate::types::common_type;
use crate::types::Type;

#[cfg(not(target_arch = "wasm32"))]
pub mod http;

pub mod ecdsa;
pub mod hash;

pub mod physics;

#[cfg(not(any(target_arch = "wasm32", target_os = "ios")))]
pub mod browse;

pub mod casting;

pub mod svg;

pub mod eth;

pub mod csv;

pub mod substrate;

pub mod curve25519;

pub mod chachapoly;
