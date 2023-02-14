/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2023 Fragcolor Pte. Ltd. */

/// Implements the graph
pub(crate) mod graph;
pub(crate) use graph::*;
/// Implements the graph basic ui
pub(crate) mod graph_ui;
pub(crate) use graph_ui::*;
/// Type declarations for the different id types (node, input, output)
pub(crate) mod id_types;
pub(crate) use id_types::*;
/// Implements the index trait for the Graph type, allowing indexing by all id types
mod index_impls;
/// Implements the different nodes
pub(crate) mod node_templates;
pub(crate) use node_templates::*;
/// Implements some utilities
pub(crate) mod util;
