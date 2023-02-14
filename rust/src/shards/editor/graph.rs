/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2023 Fragcolor Pte. Ltd. */

// Some parts taken from egui_node_graph
// MIT license
// Copyright (c) 2022 setzer22

use super::id_types::*;
use slotmap::SlotMap;

pub(crate) struct Graph<NodeData> {
  pub nodes: SlotMap<NodeId, Node<NodeData>>,
}

#[derive(Clone)]
pub(crate) struct Node<NodeData> {
  pub id: NodeId,
  pub label: String,
  pub data: NodeData,
}

impl<NodeData> Default for Graph<NodeData> {
  fn default() -> Self {
    Self::new()
  }
}

impl<NodeData> Graph<NodeData> {
  pub fn new() -> Self {
    Self {
      nodes: SlotMap::default(),
    }
  }

  pub fn add_node(&mut self, label: String, data: NodeData) -> NodeId {
    self.nodes.insert_with_key(|id| Node::new(id, label, data))
  }

  pub fn remove_node(&mut self, node_id: NodeId) -> Node<NodeData> {
    self.nodes.remove(node_id).expect("Node should exist")
  }

  pub fn clear(&mut self) {
    self.nodes.clear();
  }
}

impl<NodeData> Node<NodeData> {
  pub fn new(id: NodeId, label: String, data: NodeData) -> Self {
    Self { id, label, data }
  }
}
