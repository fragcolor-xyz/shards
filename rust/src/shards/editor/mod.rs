/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2023 Fragcolor Pte. Ltd. */

/// Implements the graph
pub(crate) mod graph;
pub(crate) use graph::*;
/// Type declarations for the different id types (node, input, output)
pub(crate) mod id_types;
pub(crate) use id_types::*;
/// Implements the index trait for the Graph type, allowing indexing by all id types
mod index_impls;
/// Implements some utilities
pub(crate) mod util;
